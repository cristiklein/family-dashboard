import dashboard.framebuffer;
import dashboard.display;
import dashboard.layout;
import dashboard.config;

#include <chrono>
#include <cstdio>
#include <exception>
#include <filesystem>
#include <format>
#include <print>
#include <stdexcept>
#include <string>
#include <string_view>

using namespace std;

namespace
{

constexpr auto usage =
    R"(Usage: family-dashboard [--morning|--afternoon|--test]
                        [--partial|--fast|--full] [--config PATH]
       family-dashboard --systemd-service [--morning|--afternoon|--test]
                        [--partial|--fast|--full] [--config PATH]
       family-dashboard --systemd-timer
       family-dashboard --help

By default, select the layout automatically. Show today's plans before 17:00
and tomorrow's from 17:00 in local time. In this mode, use a full refresh at
minute 00 and partial otherwise, unless a refresh style is specified.
--morning shows today's plans. --afternoon shows tomorrow's plans.
--test draws a hardware test image without reading the configuration.
These three layouts default to partial refresh.
Use --config PATH to select a YAML file for a display run or generated service.
--systemd-service and --systemd-timer print user units with setup hints.
)";

enum class action
{
  display,
  service,
  timer
};

struct options
{
  action command = action::display;
  string_view layout;
  string_view refresh;
  string config_path;
};

options parse_options(int argc, char **argv)
{
  options result;
  for (int i = 1; i < argc; ++i)
  {
    const string_view flag = argv[i];
    if (flag == "--morning" || flag == "--afternoon" || flag == "--test")
    {
      if (!result.layout.empty())
      {
        throw invalid_argument("Choose one layout");
      }
      result.layout = flag;
    }
    else if (flag == "--partial" || flag == "--fast" || flag == "--full")
    {
      if (!result.refresh.empty())
      {
        throw invalid_argument("Choose one refresh style");
      }
      result.refresh = flag;
    }
    else if (flag == "--config")
    {
      if (!result.config_path.empty())
      {
        throw invalid_argument("Supply --config PATH once");
      }
      if (i + 1 == argc || string_view(argv[i + 1]).empty() ||
          string_view(argv[i + 1]).starts_with("--"))
      {
        throw invalid_argument("--config requires a path");
      }
      result.config_path = argv[++i];
    }
    else
    {
      if (result.command != action::display)
      {
        throw invalid_argument(format("Unexpected option {}", flag));
      }
      if (flag == "--systemd-service")
      {
        result.command = action::service;
      }
      else if (flag == "--systemd-timer")
      {
        result.command = action::timer;
      }
      else
      {
        throw invalid_argument(format("Unknown option {}", flag));
      }
    }
  }

  if (result.command == action::timer)
  {
    if (!result.layout.empty() || !result.refresh.empty())
    {
      throw invalid_argument(
          "Layout and refresh options require a display or service command");
    }
    if (!result.config_path.empty())
    {
      throw invalid_argument("Supply --config with --systemd-service");
    }
  }
  return result;
}

string quote_unit_argument(string_view argument)
{
  string quoted = "\"";
  for (const unsigned char ch : argument)
  {
    switch (ch)
    {
    case '%':
      quoted += "%%";
      break;
    case '\\':
    case '"':
      quoted += '\\';
      quoted += static_cast<char>(ch);
      break;
    default:
      if (ch < 0x20 || ch == 0x7f)
      {
        quoted += format("\\x{:02x}", ch);
      }
      else
      {
        quoted += static_cast<char>(ch);
      }
    }
  }
  return quoted + '"';
}

void print_service(const options &opts)
{
  const auto config =
      opts.config_path.empty()
          ? "\"%h/.config/family-dashboard/dashboard.yaml\""
          : quote_unit_argument(
                filesystem::absolute(opts.config_path).string());

  // The ExecStart colon prefix keeps dollar signs in paths literal.
  print(
      R"(# Save as ~/.config/systemd/user/family-dashboard.service

[Unit]
Description=Refresh the family dashboard

[Service]
Type=oneshot
StandardOutput=journal
StandardError=journal
ExecStart=":%h/.local/bin/family-dashboard")");
  if (!opts.layout.empty())
  {
    print(" {}", opts.layout);
  }
  if (!opts.refresh.empty())
  {
    print(" {}", opts.refresh);
  }
  println(" --config {}", config);
}

void print_timer()
{
  print(
      R"(# Save as ~/.config/systemd/user/family-dashboard.timer alongside the service.
# systemctl --user daemon-reload
# systemctl --user enable --now family-dashboard.timer

[Unit]
Description=Refresh the family dashboard every minute

[Timer]
OnCalendar=*-*-* *:*:00
AccuracySec=1us
Unit=family-dashboard.service

[Install]
WantedBy=timers.target
)");
}

void display(const options &opts)
{
  const auto now = chrono::system_clock::now();
  const auto scheduled = dashboard::refresh_schedule_at(now);
  const bool automatic = opts.layout.empty();
  const bool test = opts.layout == "--test";
  const bool afternoon =
      automatic ? scheduled.afternoon : opts.layout == "--afternoon";
  const auto refresh =
      opts.refresh.empty()
          ? (automatic && scheduled.full ? "--full" : "--partial")
          : opts.refresh;
  const auto mode = refresh == "--fast"   ? dashboard::refresh_mode::fast
                    : refresh == "--full" ? dashboard::refresh_mode::full
                                          : dashboard::refresh_mode::partial;

  const auto image = test ? dashboard::test_image()
                          : dashboard::configured_dashboard(
                                dashboard::load_configuration(opts.config_path),
                                afternoon, now);
  println("Refreshing Waveshare 7.5-inch V2 (800x480), {} layout, {} mode...",
          test        ? "test"
          : afternoon ? "afternoon"
                      : "morning",
          refresh.substr(2));
  fflush(stdout);
  dashboard::show_on_display(image, mode);
  println("Refresh finished; display is asleep.");
}

void run(const options &opts)
{
  switch (opts.command)
  {
  case action::display:
    display(opts);
    break;
  case action::service:
    print_service(opts);
    break;
  case action::timer:
    print_timer();
    break;
  }
}

} // namespace

int main(int argc, char **argv)
{
  try
  {
    if (argc == 2 && string_view(argv[1]) == "--help")
    {
      print("{}", usage);
    }
    else
    {
      run(parse_options(argc, argv));
    }
    return fflush(stdout) == 0 ? 0 : 1;
  }
  catch (const invalid_argument &error)
  {
    println(stderr, "Error: {}. See family-dashboard --help.", error.what());
    return 2;
  }
  catch (const exception &error)
  {
    println(stderr, "Error: {}", error.what());
    return 1;
  }
}
