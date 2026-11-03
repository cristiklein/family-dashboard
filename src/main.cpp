#include <cstdio>
#include <exception>
#include <print>
#include <string_view>

import dashboard.greeting;
import dashboard.framebuffer;
import dashboard.display;
import dashboard.layout;

using namespace std;

int main(int argc, char **argv)
{
  try
  {
    if (argc == 1)
    {
      println("{}", dashboard::greeting());
    }
    else if (argc == 3 && string_view(argv[1]) == "--preview")
    {
      dashboard::test_image().write_pbm(argv[2]);
      println("Wrote 800x480 preview to {}", argv[2]);
    }
    else if (argc == 3 && (string_view(argv[1]) == "--preview-morning" ||
                           string_view(argv[1]) == "--preview-afternoon"))
    {
      dashboard::sample_dashboard(string_view(argv[1]) == "--preview-afternoon")
          .write_pbm(argv[2]);
      println("Wrote 800x480 sample dashboard to {}", argv[2]);
    }
    else if (argc == 2 && string_view(argv[1]) == "--demo-partial")
    {
      dashboard::show_partial_demo(dashboard::sample_dashboard(false),
                                   dashboard::sample_dashboard(true));
      println("Partial demo finished; display is asleep.");
    }
    else if ((argc == 2 ||
              (argc == 3 && (string_view(argv[2]) == "--fast" ||
                             string_view(argv[2]) == "--full" ||
                             string_view(argv[2]) == "--partial"))) &&
             (string_view(argv[1]) == "--display" ||
              string_view(argv[1]) == "--display-morning" ||
              string_view(argv[1]) == "--display-afternoon"))
    {
      const auto refresh = argc == 3 ? string_view(argv[2]) : "--partial";
      const auto mode = refresh == "--fast" ? dashboard::refresh_mode::fast
                        : refresh == "--full"
                            ? dashboard::refresh_mode::full
                            : dashboard::refresh_mode::partial;
      println("Refreshing Waveshare 7.5-inch V2 (800x480), {} mode...",
              refresh.substr(2));
      if (fflush(stdout) != 0)
      {
        return 1;
      }
      const auto option = string_view(argv[1]);
      dashboard::show_on_display(
          option == "--display"
              ? dashboard::test_image()
              : dashboard::sample_dashboard(option == "--display-afternoon"),
          mode);
      println("Refresh finished; display is asleep.");
    }
    else
    {
      println(stderr,
              "Usage: {} [--preview image.pbm | --preview-morning image.pbm | "
              "--preview-afternoon image.pbm | "
              "(--display | --display-morning | --display-afternoon) "
              "[--partial|--fast|--full] | --demo-partial]",
              argv[0]);
      return 2;
    }
    return fflush(stdout) == 0 ? 0 : 1;
  }
  catch (const exception &error)
  {
    println(stderr, "Error: {}", error.what());
    return 1;
  }
}
