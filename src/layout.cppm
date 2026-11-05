module;

#include <array>
#include <chrono>
#include <ctime>
#include <format>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

export module dashboard.layout;

import dashboard.framebuffer;
import dashboard.config;

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

// Count UTF-8 characters so Swedish names do not wrap by byte count.
size_t text_length(string_view text)
{
  size_t count = 0;
  for (unsigned char byte : text)
  {
    if ((byte & 0xc0) != 0x80)
    {
      ++count;
    }
  }
  return count;
}

string take_text(string_view text, size_t count)
{
  size_t end = 0;
  size_t seen = 0;
  while (end < text.size())
  {
    if ((static_cast<unsigned char>(text[end]) & 0xc0) != 0x80 &&
        seen++ == count)
    {
      break;
    }
    ++end;
  }
  return string(text.substr(0, end));
}

void wrapped_text(dashboard::mono_image &image, string text, int x, int y,
                  size_t columns, int lines)
{
  for (int line = 0; line < lines && !text.empty(); ++line)
  {
    if (text_length(text) <= columns)
    {
      dashboard::draw_text(image, text, x, y + line * 18, 2);
      return;
    }
    if (line == lines - 1)
    {
      dashboard::draw_text(image, take_text(text, columns - 3) + "...", x,
                           y + line * 18, 2);
      return;
    }
    auto part = take_text(text, columns);
    const auto space = part.find_last_of(' ');
    if (space != string::npos && space > 0)
    {
      part.resize(space);
    }
    dashboard::draw_text(image, part, x, y + line * 18, 2);
    text.erase(0, part.size());
    const auto start = text.find_first_not_of(' ');
    text = start == string::npos ? "" : text.substr(start);
  }
}

} // namespace

export namespace dashboard
{

// Accept a time explicitly so layout tests can render reproducible images.
mono_image configured_dashboard(
    const configuration &config, bool afternoon,
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

  const auto summaries = schedule_for(config, agenda_day);
  bool changed = false;
  for (size_t i = 0; i < summaries.size(); ++i)
  {
    const auto &item = summaries[i];
    const int y = 130 + static_cast<int>(i) * 60;
    const size_t name_limit = item.changed ? 24 : 26;
    const auto name = text_length(item.name) > name_limit
                          ? take_text(item.name, name_limit - 3) + "..."
                          : item.name;
    draw_text(image, name + (item.changed ? " *" : ""), 24, y, 3);
    wrapped_text(image, item.summary, 24, y + 25, 38, 2);
    changed = changed || item.changed;
  }

  draw_text(image, "GOOD THINGS AHEAD", 532, 90, 2);
  size_t shown = 0;
  size_t upcoming = 0;
  for (const auto &thing : config.good_things)
  {
    if (thing.date < today)
    {
      continue;
    }
    ++upcoming;
    if (shown >= 3)
    {
      continue;
    }
    const int y = 130 + static_cast<int>(shown) * 76;
    const auto days = (thing.date - today).count();
    draw_text(image,
              days == 0 ? "Today!"
                        : format("{} {}", days, days == 1 ? "day" : "days"),
              532, y, 3);
    wrapped_text(image, thing.title, 532, y + 27, 20, 2);
    ++shown;
  }
  if (upcoming == 0)
  {
    wrapped_text(image, "Nothing planned yet", 532, 134, 20, 2);
  }
  if (upcoming > shown)
  {
    draw_text(image, format("+{} more ahead", upcoming - shown), 532, 362, 2);
  }

  image.rectangle(24, 400, 752, 2);
  draw_text(image, "DEVIATIONS", 24, 422, 2);
  draw_text(image,
            changed ? "* Changed from the usual weekly schedule"
                    : "Following the usual weekly schedule",
            24, 450, 2);
  return image;
}

} // namespace dashboard
