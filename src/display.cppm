module;

#include <algorithm>
#include <array>
#include <cerrno>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <initializer_list>
#include <print>
#include <span>
#include <stdexcept>
#include <string>
#include <system_error>
#include <thread>
#include <vector>

#ifdef __linux__
#include <fcntl.h>
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

[[noreturn]] void fail(const string &operation)
{
  const auto code = errno;
  throw system_error(code, generic_category(), operation);
}

class file_handle
{
  int fd_;

public:
  explicit file_handle(const string &path, int flags)
      : fd_(::open(path.c_str(), flags | O_CLOEXEC))
  {
    if (fd_ < 0)
    {
      fail("open " + path);
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

void write_file(const string &path, const string &text)
{
  file_handle file(path, O_WRONLY);
  if (::write(file.get(), text.data(), text.size()) !=
      static_cast<ssize_t>(text.size()))
  {
    fail("write " + path);
  }
}

void delay(int ms)
{
  this_thread::sleep_for(chrono::milliseconds(ms));
}

class gpio_pin
{
  int number_;
  string path_;
  bool output_;

public:
  gpio_pin(int number, const char *direction)
      : number_(number), path_("/sys/class/gpio/gpio" + to_string(number)),
        output_(strcmp(direction, "in") != 0)
  {
    // Refuse to take over an export belonging to another process.
    write_file("/sys/class/gpio/export", to_string(number_));
    try
    {
      write_file(path_ + "/direction", direction);
    }
    catch (...)
    {
      unexport();
      throw;
    }
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
    unexport();
  }
  gpio_pin(const gpio_pin &) = delete;
  gpio_pin &operator=(const gpio_pin &) = delete;

  void set(bool high)
  {
    write_file(path_ + "/value", high ? "1" : "0");
  }
  bool high() const
  {
    file_handle file(path_ + "/value", O_RDONLY);
    char value;
    if (::read(file.get(), &value, 1) != 1)
    {
      fail("read " + path_);
    }
    if (value != '0' && value != '1')
    {
      throw runtime_error("Invalid GPIO input");
    }
    return value == '1';
  }

private:
  void unexport() noexcept
  {
    try
    {
      write_file("/sys/class/gpio/unexport", to_string(number_));
    }
    catch (...)
    {
    }
  }
};

class spi_bus
{
  file_handle device_{"/dev/spidev2.0", O_RDWR};

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
      fail("configure SPI");
    }
  }
  void send(span<const uint8_t> bytes)
  {
    while (!bytes.empty())
    {
      // Small full-duplex transfers avoid the old Rockchip TX-only DMA path.
      // MISO is unused by the panel; received bytes are discarded.
      array<uint8_t, 32> received{};
      const auto size = min<size_t>(bytes.size(), received.size());
      spi_ioc_transfer transfer{};
      transfer.tx_buf = reinterpret_cast<uintptr_t>(bytes.data());
      transfer.rx_buf = reinterpret_cast<uintptr_t>(received.data());
      transfer.len = size;
      transfer.bits_per_word = 8;
      transfer.speed_hz = 500'000;
      if (::ioctl(device_.get(), SPI_IOC_MESSAGE(1), &transfer) !=
          static_cast<int>(size))
      {
        fail("SPI transfer");
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
  gpio_pin power_{184, "high"}; // Physical pin 12; newer HAT power enable.
  gpio_pin reset_{164, "high"}; // Physical pin 11.
  gpio_pin dc_{171, "low"};     // Physical pin 22.
  gpio_pin busy_{163, "in"};    // Physical pin 18; active LOW.
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

  void partial_demo(const dashboard::mono_image &first,
                    const dashboard::mono_image &second)
  {
    println("Drawing initial image with partial refresh...");
    refresh(first, dashboard::refresh_mode::partial);
    println("Initial image ready. Partial update in 5 seconds...");
    fflush(stdout);
    delay(5000);
    refresh(second, dashboard::refresh_mode::partial);
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
  throw runtime_error("Display output requires Linux; use --preview on macOS");
#endif
}

// Two partial updates in one session; no initial fast/full cleaning refresh.
void show_partial_demo(const mono_image &first, const mono_image &second)
{
#ifdef __linux__
  epaper panel;
  panel.partial_demo(first, second);
#else
  throw runtime_error("Display output requires Linux; use --preview on macOS");
#endif
}

} // namespace dashboard
