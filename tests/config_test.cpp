#include <chrono>
#include <cstdio>
#include <exception>
#include <print>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>

import dashboard.config;

using namespace std;

namespace
{

const string members = R"(members:
  - {id: a, name: Åsa}
  - {id: b, name: Sam}
  - {id: c, name: Charlie}
  - {id: d, name: Robin}
)";

void require(bool condition, string_view message)
{
  if (!condition)
  {
    throw runtime_error(string(message));
  }
}

void rejects(const string &text)
{
  istringstream input(text);
  try
  {
    dashboard::parse_configuration(input);
  }
  catch (const runtime_error &)
  {
    return;
  }
  throw runtime_error("Invalid configuration was accepted: " + text);
}

} // namespace

int main()
{
  try
  {
    istringstream input(members + R"(
# Comment
weekly:
  thursday:
    a: "Office; pickup = 17:00" # Inline comment
    b: 'Swimming'
deviations:
  "2026-11-05":
    a: Day off
good-things:
  - date: 2027-01-01
    title: New year
  - date: "2026-11-06"
    title: >-
      Movie
      night
  - {date: "2026-11-06", title: 'Dinner: at Grandma''s'}
)");
    const auto config = dashboard::parse_configuration(input);
    const auto day = dashboard::parse_date("2026-11-05");
    const auto changed = dashboard::schedule_for(config, day);
    require(changed.size() == 4 && changed[0].name == "Åsa",
            "Preserve member order and UTF-8 names");
    require(changed[0].changed && changed[0].summary == "Day off",
            "Deviation must replace a member's usual schedule");
    require(!changed[1].changed && changed[1].summary == "Swimming",
            "Deviation must leave other members' schedules intact");
    require(changed[2].summary == "No plans",
            "Missing weekday summary needs a fallback");
    const auto usual = dashboard::schedule_for(config, day + chrono::days{7});
    require(!usual[0].changed && usual[0].summary == "Office; pickup = 17:00",
            "Overrides must apply only to their exact date; text is literal");
    require(config.good_things[0].title == "Movie night",
            "Good things must be ordered by date");
    require(dashboard::schedule_for(config, day + chrono::days{3})[0].summary ==
                "No plans",
            "Sunday must map to the correct weekday");
    require(config.good_things[1].title == "Dinner: at Grandma's",
            "Support quoted text and multiple good things on the same date");
    rejects("members: [{id: a, name: Alex}]\n");
    rejects(members + "weekly: {monday: {a: One, a: Two}}\n");
    rejects(members + "weekly: {monday: {unknown: Office}}\n");
    rejects(members + "weekly: {monady: {a: Office}}\n");
    rejects(members + "deviations: {2026-02-29: {a: Day off}}\n");
    rejects(members + "good-things: [{date: 2026-13-01, title: Holiday}]\n");
    rejects(members + "good-things: [{date: 2026-1-01, title: Holiday}]\n");
    rejects(members + "weekly: {monday: {a: null}}\n");
    rejects(members + "weekly: {monday: {a: []}}\n");
    rejects(members + "weekly: {monday: {a: '   '}}\n");
    rejects(members + "weekly: [monday]\n");
    rejects(members + "weekly: {monday: [Office]}\n");
    rejects(members + "weekly: {}\nweekly: {}\n");
    rejects(members + "deviations: {2026-11-05: {}, '2026-11-05': {}}\n");
    rejects(members + "good-things: [{date: 2026-11-06}]\n");
    rejects(members +
            "good-things: [{date: 2026-11-06, title: Fun, typo: x}]\n");
    rejects(members + "unknown: {}\n");
    rejects(members + "weekly: [broken\n");
    rejects(members + "---\nweekly: {}\n");
    rejects("members: [{id: a, name: A}, {id: a, name: B}, {id: c, name: C}, "
            "{id: d, name: D}]");
    rejects("members: [{id: a}, {id: b, name: B}, {id: c, name: C}, {id: d, "
            "name: D}]");
    rejects("[members]\na = Alex\n");
    return 0;
  }
  catch (const exception &error)
  {
    println(stderr, "{}", error.what());
    return 1;
  }
}
