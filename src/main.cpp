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
    R"(Usage: family-dashboard [--auto|--morning|--afternoon|--test]
                        [--partial|--fast|--full] [--config PATH]
       family-dashboard --systemd-service [layout and refresh options] [--config PATH]
       family-dashboard --systemd-timer
       family-dashboard --preview image.pbm
       family-dashboard (--preview-morning|--preview-afternoon) image.pbm [--config PATH]
       family-dashboard --demo-partial [--config PATH]
       family-dashboard --help

With no options, display today's plans before 17:00 and tomorrow's from
17:00 in local time. Use a full refresh at minute 00 and partial otherwise.
--morning, --afternoon and --test default to partial refresh.
--systemd-service and --systemd-timer print user units and installation commands.
)";

enum class action
{
  display,
  preview,
  preview_morning,
  preview_afternoon,
  partial_demo,
  service,
  timer
};

struct options
{
  action command = action::display;
  string_view layout;
  string_view refresh;
  string preview_path;
  string config_path;
};

string_view layout_flag(string_view flag)
{
  if (flag == "--auto" || flag == "--display-auto")
  {
    return "--auto";
  }
  if (flag == "--morning" || flag == "--display-morning")
  {
    return "--morning";
  }
  if (flag == "--afternoon" || flag == "--display-afternoon")
  {
    return "--afternoon";
  }
  return flag == "--test" || flag == "--display" ? "--test" : string_view{};
}

options parse_options(int argc, char **argv)
{
  options result;
  auto read_path = [&](int &i, string_view flag)
  {
    if (i + 1 == argc || string_view(argv[i + 1]).empty() ||
        string_view(argv[i + 1]).starts_with("--"))
    {
      throw invalid_argument(format("{} requires a path", flag));
    }
    return argv[++i];
  };

  for (int i = 1; i < argc; ++i)
  {
    const string_view flag = argv[i];
    const auto layout = layout_flag(flag);
    if (!layout.empty())
    {
      if (!result.layout.empty())
      {
        throw invalid_argument("Choose one layout");
      }
      result.layout = layout;
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
      result.config_path = read_path(i, flag);
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
      else if (flag == "--demo-partial")
      {
        result.command = action::partial_demo;
      }
      else if (flag == "--preview" || flag == "--preview-morning" ||
               flag == "--preview-afternoon")
      {
        result.preview_path = read_path(i, flag);
        result.command = flag == "--preview" ? action::preview
                         : flag == "--preview-morning"
                             ? action::preview_morning
                             : action::preview_afternoon;
      }
      else
      {
        throw invalid_argument(format("Unknown option {}", flag));
      }
    }
  }

  if (result.command != action::display && result.command != action::service &&
      (!result.layout.empty() || !result.refresh.empty()))
  {
    throw invalid_argument(
        "Layout and refresh options require a display or service command");
  }
  if (result.command == action::timer && !result.config_path.empty())
  {
    throw invalid_argument("Supply --config with --systemd-service");
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
      "{}",
      R"(# Save this as family-dashboard.service, then run as the dashboard user.
# unit_dir="${XDG_CONFIG_HOME:-$HOME/.config}/systemd/user"
# install -D -m 644 family-dashboard.service "$unit_dir/family-dashboard.service"
# systemctl --user daemon-reload
# systemctl --user start family-dashboard.service
# journalctl --user -u family-dashboard.service -f
# For automatic updates, also generate and install the --systemd-timer unit.

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
      "{}",
      R"(# Install the user service first, then save this as family-dashboard.timer.
# unit_dir="${XDG_CONFIG_HOME:-$HOME/.config}/systemd/user"
# install -D -m 644 family-dashboard.timer "$unit_dir/family-dashboard.timer"
# systemctl --user daemon-reload
# systemctl --user enable --now family-dashboard.timer
# systemctl --user list-timers family-dashboard.timer
# To run after logout and at boot:
# sudo loginctl enable-linger "$USER"
# To stop scheduled updates:
# systemctl --user disable --now family-dashboard.timer

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
  const bool automatic = opts.layout.empty() || opts.layout == "--auto";
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
  case action::preview:
    dashboard::test_image().write_pbm(opts.preview_path);
    println("Wrote 800x480 preview to {}", opts.preview_path);
    break;
  case action::preview_morning:
  case action::preview_afternoon:
    dashboard::configured_dashboard(
        dashboard::load_configuration(opts.config_path),
        opts.command == action::preview_afternoon)
        .write_pbm(opts.preview_path);
    println("Wrote 800x480 dashboard to {}", opts.preview_path);
    break;
  case action::partial_demo:
  {
    const auto config = dashboard::load_configuration(opts.config_path);
    dashboard::show_partial_demo(dashboard::configured_dashboard(config, false),
                                 dashboard::configured_dashboard(config, true));
    println("Partial demo finished; display is asleep.");
    break;
  }
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
