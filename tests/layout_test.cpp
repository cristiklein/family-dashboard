#include <chrono>
#include <cstdio>
#include <ctime>
#include <exception>
#include <print>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

import dashboard.framebuffer;
import dashboard.layout;
import dashboard.config;

using namespace std;

namespace
{

const dashboard::configuration config{
    {{"a", "Alex"}, {"b", "Sam"}, {"c", "Charlie"}, {"d", "Robin"}},
    {},
    {},
    {}};

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

chrono::system_clock::time_point local_time(int year, int month, int day,
                                            int hour = 7, int minute = 9)
{
  tm local{};
  local.tm_year = year - 1900;
  local.tm_mon = month - 1;
  local.tm_mday = day;
  local.tm_hour = hour;
  local.tm_min = minute;
  local.tm_isdst = -1;
  const auto timestamp = mktime(&local);
  require(timestamp != -1, "Cannot create local test time");
  return chrono::system_clock::from_time_t(timestamp);
}

void require_label(const dashboard::mono_image &image, string_view text, int x,
                   int y, int scale)
{
  dashboard::mono_image expected;
  dashboard::draw_text(expected, text, x, y, scale);
  const int width = (static_cast<int>(text.size()) * 6 - 1) * scale;
  for (int row = y; row < y + 7 * scale; ++row)
  {
    for (int col = x; col < x + width; ++col)
    {
      require(black(image, col, row) == black(expected, col, row),
              "Date label does not match the expected local calendar date");
    }
  }
}

void test_dates()
{
  const auto check =
      [](auto now, string_view header, string_view today, string_view tomorrow)
  {
    const auto morning = dashboard::configured_dashboard(config, false, now);
    const auto afternoon = dashboard::configured_dashboard(config, true, now);
    require_label(morning, header, 658, 46, 2);
    require_label(afternoon, header, 658, 46, 2);
    require_label(morning, today, 24, 90, 3);
    require_label(afternoon, tomorrow, 24, 90, 3);
  };
  check(local_time(2026, 11, 3), "TUE 03 NOV", "TODAY / TUE 03",
        "TOMORROW / WED 04");
  check(local_time(2026, 11, 30), "MON 30 NOV", "TODAY / MON 30",
        "TOMORROW / TUE 01");
  check(local_time(2026, 12, 31), "THU 31 DEC", "TODAY / THU 31",
        "TOMORROW / FRI 01");
  check(local_time(2024, 2, 28), "WED 28 FEB", "TODAY / WED 28",
        "TOMORROW / THU 29");
  check(local_time(2024, 2, 29), "THU 29 FEB", "TODAY / THU 29",
        "TOMORROW / FRI 01");
  check(local_time(2025, 2, 28), "FRI 28 FEB", "TODAY / FRI 28",
        "TOMORROW / SAT 01");
  // In Stockholm these cross the spring and autumn daylight-saving changes.
  check(local_time(2026, 3, 28, 23, 30), "SAT 28 MAR", "TODAY / SAT 28",
        "TOMORROW / SUN 29");
  check(local_time(2026, 10, 25, 0, 30), "SUN 25 OCT", "TODAY / SUN 25",
        "TOMORROW / MON 26");
  check(local_time(2026, 11, 3, 23, 59), "TUE 03 NOV", "TODAY / TUE 03",
        "TOMORROW / WED 04");
  check(local_time(2026, 11, 4, 0, 0), "WED 04 NOV", "TODAY / WED 04",
        "TOMORROW / THU 05");
}

void test_configured_content()
{
  auto plans = config;
  plans.weekly[2]["a"] = "Office";
  const auto today = dashboard::parse_date("2026-11-04");
  plans.deviations[today + chrono::days{1}]["a"] = "Day off";
  plans.good_things = {{today - chrono::days{1}, "Past"},
                       {today, "Movie night"},
                       {today + chrono::days{1}, "Swimming"}};
  const auto now = local_time(2026, 11, 4);
  const auto morning = dashboard::configured_dashboard(plans, false, now);
  const auto afternoon = dashboard::configured_dashboard(plans, true, now);
  require_label(morning, "Alex", 24, 130, 3);
  require_label(morning, "Office", 24, 155, 2);
  require_label(afternoon, "Alex *", 24, 130, 3);
  require_label(afternoon, "Day off", 24, 155, 2);
  for (const auto *image : {&morning, &afternoon})
  {
    require_label(*image, "Today!", 532, 130, 3);
    require_label(*image, "Movie night", 532, 157, 2);
    require_label(*image, "1 day", 532, 206, 3);
    require_label(*image, "Swimming", 532, 233, 2);
  }
  // Long words and UTF-8 summaries must stay inside their column.
  plans.weekly[2]["a"] = string(100, 'X') + " ÅÄÖåäö";
  const auto long_text = dashboard::configured_dashboard(plans, false, now);
  for (int y = 155; y < 187; ++y)
  {
    for (int x = 480; x < 506; ++x)
    {
      require(!black(long_text, x, y),
              "Long text must not cross the schedule column");
    }
  }
}

void test_clock(bool afternoon)
{
  using namespace chrono;
  const auto start = local_time(2026, 11, 3);
  const auto first = dashboard::configured_dashboard(config, afternoon, start);
  require_clock(first, "07:09");
  require(first.pixels == dashboard::configured_dashboard(config, afternoon,
                                                          start + seconds(59))
                              .pixels,
          "Seconds must not change the rendered layout");
  const auto next =
      dashboard::configured_dashboard(config, afternoon, start + minutes(1));
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
  require_clock(dashboard::configured_dashboard(
                    config, afternoon, local_time(2026, 11, 3, 23, 59)),
                "23:59");
  require_clock(dashboard::configured_dashboard(config, afternoon,
                                                local_time(2026, 11, 4, 0, 0)),
                "00:00");
}

} // namespace

int main()
{
  try
  {
    test_clock(false);
    test_clock(true);
    test_dates();
    test_configured_content();
    return 0;
  }
  catch (const exception &error)
  {
    println(stderr, "{}", error.what());
    return 1;
  }
}
