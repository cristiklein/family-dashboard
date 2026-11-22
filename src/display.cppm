module;

#include <algorithm>
#include <cerrno>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <exception>
#include <filesystem>
#include <format>
#include <initializer_list>
#include <print>
#include <span>
#include <stdexcept>
#include <string>
#include <string_view>
#include <system_error>
#include <thread>
#include <vector>

#ifdef __linux__
#include <fcntl.h>
#include <linux/gpio.h>
#include <linux/spi/spidev.h>
#include <sys/file.h>
#include <sys/ioctl.h>
#include <unistd.h>
#endif

export module dashboard.display;

import dashboard.framebuffer;

using namespace std;

export namespace dashboard
{

enum class refresh_mode
{
  full,
  fast,
  partial
};

} // namespace dashboard

#ifdef __linux__
namespace
{

[[noreturn]] void fail(const string &operation, string_view group = {})
{
  const error_code code(errno, generic_category());
  if (!group.empty() && (code == errc::permission_denied ||
                         code == errc::operation_not_permitted))
  {
    throw system_error(
        code, format("{} (check device permissions and your groups with id. "
                     "If needed, run sudo usermod -aG {} \"$USER\" on the Pi "
                     "and log out and back in)",
                     operation, group));
  }
  if (group == "spi" && code == errc::no_such_file_or_directory)
  {
    throw system_error(
        code,
        operation +
            " (enable SPI with sudo raspi-config nonint do_spi 0 and reboot)");
  }
  throw system_error(code, operation);
}

class file_handle
{
  int fd_;

public:
  explicit file_handle(int fd) : fd_(fd)
  {
  }
  file_handle(const string &path, int flags, string_view group)
      : fd_(::open(path.c_str(), flags | O_CLOEXEC))
  {
    if (fd_ < 0)
    {
      fail("open " + path, group);
    }
  }
  ~file_handle()
  {
    ::close(fd_);
  }
  file_handle(const file_handle &) = delete;
  file_handle &operator=(const file_handle &) = delete;
  int get() const
  {
    return fd_;
  }
};

string gpio_chip_path()
{
  // GPIO chip numbering is not an ABI. Match the Zero 2 W's controller label.
  exception_ptr permission_error;
  for (const auto &entry : filesystem::directory_iterator("/dev"))
  {
    if (!entry.path().filename().string().starts_with("gpiochip"))
    {
      continue;
    }
    const auto path = entry.path().string();
    try
    {
      file_handle chip(path, O_RDONLY, "gpio");
      gpiochip_info info{};
      if (::ioctl(chip.get(), GPIO_GET_CHIPINFO_IOCTL, &info) < 0)
      {
        fail("inspect GPIO chip " + path, "gpio");
      }
      if (string_view(info.label) == "pinctrl-bcm2835")
      {
        return path;
      }
    }
    catch (const system_error &error)
    {
      if (error.code() != errc::permission_denied &&
          error.code() != errc::operation_not_permitted)
      {
        throw;
      }
      permission_error = current_exception();
    }
  }
  if (permission_error)
  {
    rethrow_exception(permission_error);
  }
  throw runtime_error("Raspberry Pi Zero GPIO controller not found. "
                      "Check for /dev/gpiochip* on the Pi");
}

void delay(int ms)
{
  this_thread::sleep_for(chrono::milliseconds(ms));
}

class gpio_pin
{
  file_handle line_;
  bool output_;

public:
  gpio_pin(int chip, uint32_t number, bool output, bool initial = false)
      : line_(request_line(chip, number, output, initial)), output_(output)
  {
  }
  ~gpio_pin()
  {
    if (output_)
    {
      try
      {
        set(false);
      }
      catch (...)
      {
      }
    }
  }
  gpio_pin(const gpio_pin &) = delete;
  gpio_pin &operator=(const gpio_pin &) = delete;

  void set(bool high)
  {
    gpio_v2_line_values values{};
    values.mask = 1;
    values.bits = high ? 1 : 0;
    if (::ioctl(line_.get(), GPIO_V2_LINE_SET_VALUES_IOCTL, &values) < 0)
    {
      fail("set GPIO line", "gpio");
    }
  }
  bool high() const
  {
    gpio_v2_line_values values{};
    values.mask = 1;
    if (::ioctl(line_.get(), GPIO_V2_LINE_GET_VALUES_IOCTL, &values) < 0)
    {
      fail("read GPIO line", "gpio");
    }
    return (values.bits & 1) != 0;
  }

private:
  static int request_line(int chip, uint32_t number, bool output, bool initial)
  {
    gpio_v2_line_request request{};
    request.offsets[0] = number;
    request.num_lines = 1;
    strcpy(request.consumer, "family-dashboard");
    request.config.flags =
        output ? GPIO_V2_LINE_FLAG_OUTPUT : GPIO_V2_LINE_FLAG_INPUT;
    if (output)
    {
      // Set direction and initial value together, avoiding a startup glitch.
      request.config.num_attrs = 1;
      request.config.attrs[0].mask = 1;
      request.config.attrs[0].attr.id = GPIO_V2_LINE_ATTR_ID_OUTPUT_VALUES;
      request.config.attrs[0].attr.values = initial ? 1 : 0;
    }
    if (::ioctl(chip, GPIO_V2_GET_LINE_IOCTL, &request) < 0)
    {
      fail("request BCM GPIO " + to_string(number), "gpio");
    }
    // The request exclusively holds the line until the returned fd is closed.
    return request.fd;
  }
};

class spi_bus
{
  file_handle device_{"/dev/spidev0.0", O_RDWR, "spi"};

public:
  spi_bus()
  {
    if (::flock(device_.get(), LOCK_EX | LOCK_NB) < 0)
    {
      fail("lock SPI bus");
    }
    uint8_t mode = SPI_MODE_0;
    uint8_t bits = 8;
    uint32_t speed = 500'000;
    if (::ioctl(device_.get(), SPI_IOC_WR_MODE, &mode) < 0 ||
        ::ioctl(device_.get(), SPI_IOC_WR_BITS_PER_WORD, &bits) < 0 ||
        ::ioctl(device_.get(), SPI_IOC_WR_MAX_SPEED_HZ, &speed) < 0)
    {
      fail("configure SPI", "spi");
    }
  }
  void send(span<const uint8_t> bytes)
  {
    while (!bytes.empty())
    {
      // Stay within spidev's default 4096-byte transfer buffer. The panel
      // accepts consecutive transfers while DC remains high; MISO is unused.
      const auto size = min<size_t>(bytes.size(), 4096);
      spi_ioc_transfer transfer{};
      transfer.tx_buf = reinterpret_cast<uintptr_t>(bytes.data());
      transfer.len = size;
      transfer.bits_per_word = 8;
      transfer.speed_hz = 500'000;
      if (::ioctl(device_.get(), SPI_IOC_MESSAGE(1), &transfer) !=
          static_cast<int>(size))
      {
        fail("SPI transfer", "spi");
      }
      bytes = bytes.subspan(size);
    }
  }
};

// Register sequence adapted from Waveshare's EPD_7in5_V2.c, V3.0 (2023-12-18).
// See third_party/Waveshare-LICENSE.txt. Full and fast initialization use the
// panel's OTP waveforms; fast follows EPD_7IN5_V2_Init_Fast. Partial updates
// follow Init_Part / Display_Part, with white=1 as in the Python driver.
class epaper
{
  spi_bus spi_;
  file_handle gpio_chip_{gpio_chip_path(), O_RDONLY, "gpio"};
  gpio_pin power_{gpio_chip_.get(), 18, true, true}; // Physical pin 12.
  gpio_pin reset_{gpio_chip_.get(), 17, true, true}; // Physical pin 11.
  gpio_pin dc_{gpio_chip_.get(), 25, true};          // Physical pin 22.
  gpio_pin busy_{gpio_chip_.get(), 24, false}; // Physical pin 18; active LOW.
public:
  ~epaper()
  {
    // Also cut power after errors, including a disconnected BUSY line timeout.
    try
    {
      reset_.set(false);
      power_.set(false);
    }
    catch (...)
    {
    }
  }
  void show(const dashboard::mono_image &image, dashboard::refresh_mode mode)
  {
    refresh(image, mode);
    sleep();
  }

private:
  void refresh_partial(const dashboard::mono_image &image)
  {
    reset();
    command(0x00, {0x1f});
    command(0x04);
    delay(100);
    wait_ready("power on");
    command(0xe0, {0x02});
    command(0xe5, {0x6e});
    command(0x50, {0xa9, 0x07});
    command(0x91); // Enter partial mode.
    // Inclusive window: x=0..799, y=0..479. Byte-aligned full-panel window.
    command(0x90, {0x00, 0x00, 0x03, 0x1f, 0x00, 0x00, 0x01, 0xdf, 0x01});
    command(0x13);
    // Partial data has the opposite polarity from our full/fast 0x13 data.
    data(image.pixels);
    turn_on_display();
  }

  void refresh(const dashboard::mono_image &image, dashboard::refresh_mode mode)
  {
    if (mode == dashboard::refresh_mode::partial)
    {
      refresh_partial(image);
      return;
    }
    initialize(mode);
    command(0x10);
    data(image.pixels);
    auto inverted = image.pixels;
    for (auto &byte : inverted)
    {
      byte ^= 0xff;
    }
    command(0x13);
    data(inverted);
    turn_on_display();
  }

  void turn_on_display()
  {
    command(0x12);
    delay(100);
    wait_ready("refresh");
  }

  void sleep()
  {
    command(0x50, {0xf7});
    command(0x02);
    wait_ready("power off");
    command(0x07, {0xa5}); // Deep sleep; the image remains visible.
  }

  void reset()
  {
    reset_.set(true);
    delay(20);
    reset_.set(false);
    delay(2);
    reset_.set(true);
    delay(20);
  }

  void initialize(dashboard::refresh_mode mode)
  {
    reset();

    if (mode == dashboard::refresh_mode::fast)
    {
      command(0x00, {0x1f}); // Black/white, OTP waveform.
      command(0x50, {0x10, 0x07});
      command(0x04);
      delay(100);
      wait_ready("power on");
      command(0x06, {0x27, 0x27, 0x18, 0x17}); // Fast booster soft start.
      command(0xe0, {0x02});
      command(0xe5, {0x5a}); // Vendor's fast waveform temperature setting.
      return;
    }

    command(0x01, {0x07, 0x07, 0x3f, 0x3f}); // Power setting.
    command(0x06, {0x17, 0x17, 0x28, 0x17}); // Booster soft start.
    command(0x04);
    delay(100);
    wait_ready("power on");
    command(0x00, {0x1f});                   // Black/white, OTP waveform.
    command(0x61, {0x03, 0x20, 0x01, 0xe0}); // 800 x 480.
    command(0x15, {0x00});
    command(0x50, {0x10, 0x07});
    command(0x60, {0x22});
  }

  void command(uint8_t value, initializer_list<uint8_t> bytes = {})
  {
    dc_.set(false);
    spi_.send(span{&value, 1}); // SPI controller drives chip select.
    if (bytes.size())
    {
      data(span{bytes.begin(), bytes.size()});
    }
  }
  void data(span<const uint8_t> bytes)
  {
    dc_.set(true);
    spi_.send(bytes);
  }
  void wait_ready(const char *phase)
  {
    const auto start = chrono::steady_clock::now();
    const auto deadline = start + chrono::seconds(30);
    while (!busy_.high())
    {
      if (chrono::steady_clock::now() >= deadline)
      {
        throw runtime_error(string("Display BUSY timeout during ") + phase);
      }
      delay(5);
    }
    delay(5);
    const auto elapsed = chrono::duration_cast<chrono::milliseconds>(
                             chrono::steady_clock::now() - start)
                             .count();
    println("Display {} ready after {} ms", phase, elapsed);
    fflush(stdout);
  }
};

} // namespace
#endif

export namespace dashboard
{

void show_on_display(const mono_image &image,
                     refresh_mode mode = refresh_mode::partial)
{
#ifdef __linux__
  epaper panel;
  panel.show(image, mode);
#else
  throw runtime_error("Display output requires Linux");
#endif
}

} // namespace dashboard
