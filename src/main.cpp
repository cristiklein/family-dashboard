#include <cstdio>
#include <exception>
#include <print>
#include <string_view>

import dashboard.greeting;
import dashboard.framebuffer;
import dashboard.display;

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
    else if (argc == 2 && string_view(argv[1]) == "--display")
    {
      println("Refreshing Waveshare 7.5-inch V2 (800x480)...");
      if (fflush(stdout) != 0)
      {
        return 1;
      }
      dashboard::show_on_display(dashboard::test_image());
      println("Refresh finished; display is asleep.");
    }
    else
    {
      println(stderr, "Usage: {} [--preview image.pbm | --display]", argv[0]);
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
