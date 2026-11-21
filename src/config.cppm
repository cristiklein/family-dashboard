module;

#include <algorithm>
#include <array>
#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <map>
#include <set>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

#include <yaml-cpp/yaml.h>

export module dashboard.config;

using namespace std;

namespace
{

constexpr char no_plans[] = "No plans";

string scalar_text(const YAML::Node &node, string_view context,
                   string_view empty_text = {})
{
  if (node.IsScalar() &&
      node.Scalar().find_first_not_of(" \t\r\n") != string::npos)
  {
    return node.Scalar();
  }
  if (!empty_text.empty() && (node.IsNull() || node.IsScalar()))
  {
    return string(empty_text);
  }
  throw runtime_error(string(context) + " must be nonempty text");
}

void mapping(const YAML::Node &node, string_view context)
{
  if (!node.IsMap())
  {
    throw runtime_error(string(context) + " must be a mapping");
  }
  set<string> keys;
  for (const auto &entry : node)
  {
    const auto key = scalar_text(entry.first, context);
    if (!keys.insert(key).second)
    {
      throw runtime_error(string(context) + ": duplicate key " + key);
    }
  }
}

void fields(const YAML::Node &node, initializer_list<string_view> allowed,
            string_view context)
{
  mapping(node, context);
  for (const auto &entry : node)
  {
    const auto key = entry.first.Scalar();
    if (ranges::find(allowed, key) == allowed.end())
    {
      throw runtime_error(string(context) + ": unknown field " + key);
    }
  }
}

map<string, string> summaries(const YAML::Node &node, string_view context)
{
  mapping(node, context);
  map<string, string> result;
  for (const auto &entry : node)
  {
    result.emplace(entry.first.Scalar(),
                   scalar_text(entry.second, context, no_plans));
  }
  return result;
}

} // namespace

export namespace dashboard
{

struct family_member
{
  string id;
  string name;
};

struct good_thing
{
  chrono::sys_days date;
  string title;
};

struct configuration
{
  vector<family_member> members;
  array<map<string, string>, 7> weekly; // Monday first.
  map<chrono::sys_days, map<string, string>> deviations;
  vector<good_thing> good_things;
};

struct daily_summary
{
  string name;
  string summary;
  bool changed;
};

chrono::sys_days parse_date(string_view value)
{
  if (value.size() != 10 || value[4] != '-' || value[7] != '-')
  {
    throw runtime_error("Expected date YYYY-MM-DD");
  }
  int number = 0;
  for (size_t i = 0; i < value.size(); ++i)
  {
    if (i == 4 || i == 7)
    {
      continue;
    }
    if (value[i] < '0' || value[i] > '9')
    {
      throw runtime_error("Expected date YYYY-MM-DD");
    }
    number = number * 10 + value[i] - '0';
  }
  const chrono::year_month_day date{chrono::year{number / 10000},
                                    chrono::month{unsigned(number / 100 % 100)},
                                    chrono::day{unsigned(number % 100)}};
  if (!date.ok() || number / 10000 == 0)
  {
    throw runtime_error("Invalid calendar date: " + string(value));
  }
  return chrono::sys_days{date};
}

vector<daily_summary> schedule_for(const configuration &config,
                                   chrono::sys_days date)
{
  vector<daily_summary> result;
  const auto &weekly = config.weekly[chrono::weekday{date}.iso_encoding() - 1];
  const auto deviation = config.deviations.find(date);
  for (const auto &member : config.members)
  {
    auto summary = weekly.find(member.id);
    daily_summary item{member.name,
                       summary == weekly.end() ? no_plans : summary->second,
                       false};
    if (deviation != config.deviations.end())
    {
      const auto changed = deviation->second.find(member.id);
      if (changed != deviation->second.end())
      {
        item.summary = changed->second;
        item.changed = true;
      }
    }
    result.push_back(item);
  }
  return result;
}

configuration parse_configuration(istream &input)
{
  configuration result;
  YAML::Node root;
  try
  {
    const auto documents = YAML::LoadAll(input);
    if (documents.size() != 1)
    {
      throw runtime_error("Expected one YAML document");
    }
    root = documents.front();
  }
  catch (const YAML::Exception &error)
  {
    throw runtime_error(error.what());
  }
  if (input.bad())
  {
    throw runtime_error("Cannot read configuration");
  }
  fields(root, {"members", "weekly", "deviations", "good-things"},
         "configuration");
  const auto members = root["members"];
  if (!members.IsSequence() || members.size() != 4)
  {
    throw runtime_error(
        "members must be a list of exactly four family members");
  }
  set<string> ids;
  for (const auto &member : members)
  {
    fields(member, {"id", "name"}, "member");
    const auto id = scalar_text(member["id"], "member id");
    if (id.find_first_not_of("abcdefghijklmnopqrstuvwxyz0123456789_-") !=
        string::npos)
    {
      throw runtime_error("Member IDs use lowercase letters, digits, _ or -");
    }
    if (!ids.insert(id).second)
    {
      throw runtime_error("Duplicate member ID: " + id);
    }
    result.members.push_back({id, scalar_text(member["name"], "member name")});
  }
  const array<string, 7> weekdays{"monday", "tuesday",  "wednesday", "thursday",
                                  "friday", "saturday", "sunday"};
  if (const auto weekly = root["weekly"])
  {
    mapping(weekly, "weekly");
    for (const auto &entry : weekly)
    {
      const auto day = ranges::find(weekdays, entry.first.Scalar());
      if (day == weekdays.end())
      {
        throw runtime_error("Unknown weekday: " + entry.first.Scalar());
      }
      result.weekly[day - weekdays.begin()] =
          summaries(entry.second, "weekly summary");
    }
  }
  if (const auto deviations = root["deviations"])
  {
    mapping(deviations, "deviations");
    for (const auto &entry : deviations)
    {
      result.deviations.emplace(parse_date(entry.first.Scalar()),
                                summaries(entry.second, "deviation summary"));
    }
  }
  if (const auto things = root["good-things"])
  {
    if (!things.IsSequence())
    {
      throw runtime_error("good-things must be a list");
    }
    for (const auto &thing : things)
    {
      fields(thing, {"date", "title"}, "good thing");
      result.good_things.push_back(
          {parse_date(scalar_text(thing["date"], "date")),
           scalar_text(thing["title"], "title")});
    }
  }
  const auto validate = [&](const auto &entries)
  {
    for (const auto &[id, summary] : entries)
    {
      if (ranges::none_of(result.members,
                          [&](const auto &m) { return m.id == id; }))
      {
        throw runtime_error("Unknown member ID: " + id);
      }
    }
  };
  for (const auto &day : result.weekly)
  {
    validate(day);
  }
  for (const auto &[date, entries] : result.deviations)
  {
    validate(entries);
  }
  ranges::stable_sort(result.good_things, {}, &good_thing::date);
  return result;
}

configuration load_configuration(string path = {})
{
  if (path.empty())
  {
    path = "dashboard.yaml";
    if (!filesystem::exists(path))
    {
      if (const auto home = getenv("HOME"))
      {
        path = string(home) + "/.config/family-dashboard/dashboard.yaml";
      }
    }
  }
  ifstream input(path);
  if (!input)
  {
    throw runtime_error(
        "Cannot open " + path +
        "; copy dashboard.example.yaml to dashboard.yaml or use --config PATH");
  }
  try
  {
    return parse_configuration(input);
  }
  catch (const exception &error)
  {
    throw runtime_error(path + ": " + error.what());
  }
}

} // namespace dashboard
