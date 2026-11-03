#include <chrono>
#include <cstdio>
#include <exception>
#include <print>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

import dashboard.framebuffer;
import dashboard.layout;

using namespace std;

namespace
{

void require(bool condition, string_view message)
{
  if (!condition)
  {
    throw runtime_error(string(message));
  }
}

bool black(const dashboard::mono_image &image, int x, int y)
{
  return !(image.pixels[y * 100 + x / 8] & (0x80 >> (x % 8)));
}

void require_clock(const dashboard::mono_image &image, string_view text)
{
  dashboard::mono_image expected;
  dashboard::draw_text(expected, text, 660, 12, 4);
  for (int y = 12; y < 40; ++y)
  {
    for (int x = 660; x < 776; ++x)
    {
      require(black(image, x, y) == black(expected, x, y),
              "Clock must show zero-padded hours and minutes");
    }
  }
}

void test_clock(bool afternoon)
{
  using namespace chrono;
  const auto day = sys_days{2026y / November / 3};
  const auto start = day + hours(7) + minutes(9);
  const auto first = dashboard::sample_dashboard(afternoon, start);
  require_clock(first, "07:09");
  require(
      first.pixels ==
          dashboard::sample_dashboard(afternoon, start + seconds(59)).pixels,
      "Seconds must not change the rendered layout");
  const auto next = dashboard::sample_dashboard(afternoon, start + minutes(1));
  require_clock(next, "07:10");
  require(first.pixels != next.pixels, "A new minute must change the image");
  for (int y = 0; y < first.height; ++y)
  {
    for (int x = 0; x < first.width; ++x)
    {
      if (x < 660 || x >= 776 || y < 12 || y >= 40)
      {
        require(black(first, x, y) == black(next, x, y),
                "Minute updates must only change the clock region");
      }
    }
  }
  require_clock(
      dashboard::sample_dashboard(afternoon, day + hours(23) + minutes(59)),
      "23:59");
  require_clock(dashboard::sample_dashboard(afternoon, day + days(1)), "00:00");
}

} // namespace

int main()
{
  try
  {
    test_clock(false);
    test_clock(true);
    return 0;
  }
  catch (const exception &error)
  {
    println(stderr, "{}", error.what());
    return 1;
  }
}
