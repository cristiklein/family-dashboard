module;

#include <array>
#include <chrono>
#include <ctime>
#include <format>
#include <stdexcept>
#include <string>
#include <string_view>

export module dashboard.layout;

import dashboard.framebuffer;

using namespace std;

namespace
{

string date_label(chrono::sys_days date, bool include_month)
{
  constexpr array<string_view, 7> weekdays{"SUN", "MON", "TUE", "WED",
                                           "THU", "FRI", "SAT"};
  constexpr array<string_view, 12> months{"JAN", "FEB", "MAR", "APR",
                                          "MAY", "JUN", "JUL", "AUG",
                                          "SEP", "OCT", "NOV", "DEC"};
  const chrono::year_month_day civil{date};
  const auto label =
      format("{} {:02}", weekdays[chrono::weekday{date}.c_encoding()],
             static_cast<unsigned>(civil.day()));
  return include_month
             ? format("{} {}", label,
                      months[static_cast<unsigned>(civil.month()) - 1])
             : label;
}

void event(dashboard::mono_image &image, int y, string_view time,
           string_view title, string_view detail)
{
  dashboard::draw_text(image, time, 24, y + 3, 2);
  dashboard::draw_text(image, title, 110, y, 3);
  dashboard::draw_text(image, detail, 110, y + 28, 2);
}

void checkbox(dashboard::mono_image &image, int x, int y, string_view label)
{
  image.rectangle(x, y, 14, 2);
  image.rectangle(x, y + 12, 14, 2);
  image.rectangle(x, y, 2, 14);
  image.rectangle(x + 12, y, 2, 14);
  dashboard::draw_text(image, label, x + 24, y, 2);
}

} // namespace

export namespace dashboard
{

// The clock and date labels are live; other content is fictional sample data.
// Accept a time explicitly so layout tests can render reproducible images.
mono_image sample_dashboard(
    bool afternoon,
    chrono::system_clock::time_point now = chrono::system_clock::now())
{
  const auto timestamp = chrono::system_clock::to_time_t(now);
  tm local{};
  if (localtime_r(&timestamp, &local) == nullptr)
  {
    throw runtime_error("Cannot determine local time for dashboard clock");
  }
  // Advance the local civil date, rather than adding 24 hours across DST.
  const chrono::sys_days today{
      chrono::year{local.tm_year + 1900} /
      chrono::month{static_cast<unsigned>(local.tm_mon + 1)} /
      chrono::day{static_cast<unsigned>(local.tm_mday)}};
  const auto agenda_day = today + chrono::days{afternoon ? 1 : 0};
  mono_image image;
  draw_text(image, afternoon ? "Good afternoon!" : "Good morning!", 24, 20, 4);
  draw_text(image, format("{:02}:{:02}", local.tm_hour, local.tm_min), 660, 12,
            4);
  draw_text(image, date_label(today, true), 658, 46, 2);
  image.rectangle(24, 66, 752, 2);

  draw_text(image,
            format("{} / {}", afternoon ? "TOMORROW" : "TODAY",
                   date_label(agenda_day, false)),
            24, 90, 3);
  image.rectangle(506, 90, 2, 262);

  if (afternoon)
  {
    event(image, 134, "08:15", "School", "Bring library books");
    event(image, 190, "15:30", "School pickup", "Dad picks up");
    event(image, 246, "17:00", "Swimming", "Pack towel + goggles");
    event(image, 302, "18:30", "Family dinner", "Everyone at home");
  }
  else
  {
    event(image, 134, "08:15", "School", "PE kit + water bottle");
    event(image, 190, "15:30", "School pickup", "Mum picks up");
    event(image, 246, "17:00", "Football", "North pitch");
    event(image, 302, "18:30", "Family dinner", "Pasta night");

    draw_text(image, "WEATHER", 532, 90, 2);
    draw_text(image, "7°C", 532, 116, 5);
    draw_text(image, "Rain after lunch", 532, 164, 2);
    draw_text(image, "Bring waterproofs", 532, 188, 2);
    image.rectangle(532, 218, 244, 1);
  }

  const int countdown_y = afternoon ? 100 : 238;
  draw_text(image, "GOOD THINGS AHEAD", 532, countdown_y, 2);
  draw_text(image, "4", 532, countdown_y + 30, afternoon ? 8 : 5);
  draw_text(image, "days to", 594, countdown_y + 34, 2);
  draw_text(image, "Movie night", 594, countdown_y + 56, 2);
  draw_text(image, "12 days: Grandma", 532, afternoon ? 270 : 326, 2);

  image.rectangle(24, 372, 752, 2);
  draw_text(image, "REMINDERS", 24, 394, 2);
  checkbox(image, 24, 426, "Put bins out tonight");
  image.rectangle(360, 394, 1, 62);
  draw_text(image, "SHOPPING", 386, 394, 2);
  checkbox(image, 386, 426, "Milk");
  checkbox(image, 506, 426, "Bread");
  checkbox(image, 638, 426, "Apples");
  draw_text(image, "SAMPLE", 706, 464, 1);
  return image;
}

} // namespace dashboard
