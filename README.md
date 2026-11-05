# Family dashboard

C++26 family dashboard using named modules, cross-compiled directly on macOS
for Raspberry Pi Zero 2 W running 64-bit Raspberry Pi OS (Debian Trixie).
No container, VM, or compiler installation on the Pi is needed.

## Build and upload

Install the native build tools with Homebrew:

```sh
brew install llvm lld cmake ninja python@3.14
cp dashboard.example.yaml dashboard.yaml # Edit names and plans before deploying.
bash scripts/build.sh
bash scripts/deploy.sh
```

The build script uses Homebrew Clang and LLD to produce a static ARM64 Linux
executable at `build/arm64-native/family-dashboard`. On first run it downloads
Debian trixie ARM64 headers and libraries into `build/sysroot-arm64`. Packages are
SHA256-checked against the Debian package index fetched over HTTPS; package
names, versions, URLs, and hashes are recorded in `build/sysroot-arm64/packages.json`.
These target packages are extracted locally, never installed on macOS.

The deploy script defaults to `cklein@zero.local`, uploads the executable to
`~/.local/bin/family-dashboard`, and runs it. Expected output:

```text
hello world
```

An alternative SSH destination can be supplied as the first argument:

```sh
bash scripts/deploy.sh your-user@zero.local
```

SSH uses your normal keys or interactive password prompt. Passwords are not
stored in this repository.

## Dashboard configuration

The dashboard reads a hand-maintained UTF-8 YAML file. Start with the fictional
example, then replace the names and plans:

```sh
cp dashboard.example.yaml dashboard.yaml
```

`dashboard.yaml` is ignored by Git. The three kinds of content are:

- **Weekly schedule:** `members` lists exactly four people with `id` and `name`,
  in display order. `weekly` maps weekdays (`monday` through `sunday`) to member
  IDs and high-level summaries. Omitted entries show `No plans`.
- **Deviations:** `deviations` maps exact dates to member summaries that replace
  their usual plans for that day. Everyone else keeps their weekly schedule.
  Use text such as `Day off` to cancel recurring plans. Changed people have a
  `*` beside their name on the display.
- **Good things ahead:** `good-things` lists entries with `date` and `title`.
  Entries are sorted by date; past entries disappear, and the next three show
  calendar-day countdowns from today in both layouts. Multiple entries may
  share a date.

For example, after defining your four members:

```yaml
weekly:
  monday:
    alex: "Office; home at 17"
deviations:
  "2026-11-05":
    alex: "Day off"
good-things:
  - date: "2026-11-06"
    title: "Movie night"
```

Dates must be `YYYY-MM-DD`. Standard YAML quoting, inline comments, and folded
text (`>-`) are supported. Quote text containing `: ` or ` #` so it is read as
text. Duplicate keys or member IDs, unknown fields or members, invalid dates,
and empty or non-text summaries are rejected. Keep summaries short: the panel
wraps them into two lines and uses `...` beyond the available space.

The build downloads [yaml-cpp 0.8.0](https://github.com/jbeder/yaml-cpp/tree/0.8.0),
checks its SHA256 hash, and links it statically for both targets. No YAML runtime
package is needed on the Pi. CMake caches the source in each build directory;
`build/dependencies/yaml-cpp-0.8.0.tar.gz`, if present, is used as a local archive
with the same hash check.

The executable reads `dashboard.yaml` in the current directory, falling back to
`~/.config/family-dashboard/dashboard.yaml` if no local file exists. Override
this with `--config /path/to/plans.yaml` on any dashboard command. Missing or
invalid configuration produces an error before touching the display.

The live preview watches `dashboard.yaml` and reloads it when saved; it uses
`dashboard.example.yaml` until you create your own file. Deployment uploads your
local `dashboard.yaml` to `~/.config/family-dashboard/dashboard.yaml` on the Pi.
An optional second deploy argument selects another local configuration file:

```sh
bash scripts/deploy.sh your-user@zero.local /path/to/plans.yaml
```

After editing plans, deploy again to update the Pi. Configuration changes do not
require recompilation. Existing display commands and the morning timer read the
file each time they run.

## Display test

The display backend targets the Waveshare 7.5-inch **black-and-white V2,
800x480**, with its Driver HAT on the Zero 2 W's 40-pin header. The Pi needs a
soldered header (or properly wired connections). Power off before attaching the
HAT, and align pin 1 on both boards.

Once, on the Pi, enable SPI and grant your login account device access:

```sh
sudo raspi-config nonint do_spi 0
sudo usermod -aG spi,gpio "$USER"
sudo reboot
```

After reconnecting, check the devices and login groups:

```sh
ls -l /dev/spidev0.0 /dev/gpiochip*
id
```

The executable uses `/dev/spidev0.0` and discovers the GPIO controller by its
`pinctrl-bcm2835` label rather than assuming a GPIO chip number. The `spi` and
`gpio` groups allow display commands to run as your normal user without sudo. A permission error means the device permissions or login
groups need checking.

Build, deploy, and explicitly use a full refresh for the first hardware test:

```sh
bash scripts/build.sh
bash scripts/deploy.sh
bash scripts/display.sh --test --full
```

The first two commands build, upload, and print `hello world`. The third sends
a test image with a border, greeting, and checker pattern to the panel, waits
for completion, then powers down and enters deep sleep. The image remains
visible. Full and fast refresh may flash the screen. Normal display commands
still default to the experimental partial waveform; they do not perform an
automatic cleaning refresh or fallback.

| Signal | Physical header pin | BCM GPIO / SPI |
| --- | --- | --- |
| MOSI | 19 | GPIO10 / SPI0 MOSI |
| CLK | 23 | GPIO11 / SPI0 SCLK |
| CS | 24 | GPIO8 / SPI0 CE0 (`/dev/spidev0.0`) |
| RST | 11 | GPIO17 |
| DC | 22 | GPIO25 |
| BUSY | 18 | GPIO24; active LOW |
| PWR (newer HATs) | 12 | GPIO18 |

The control lines use Linux's GPIO character-device v2 API, with exclusive
line ownership and initial output levels set atomically. Lines already in use
are rejected. Outputs are driven low during normal cleanup, and closing their
file descriptors releases the line requests. BUSY waits time out after 30
seconds. SPI uses mode 0, 8-bit words, 500 kHz, and transfers of up to 4096
bytes; the kernel controls chip select. MISO is unused. No external GPIO library
is linked into the executable.

Wiring follows [Waveshare's HAT manual](https://www.waveshare.com/wiki/7.5inch_e-Paper_HAT_Manual).
Panel register settings are adapted from
[Waveshare's reference driver](https://github.com/waveshareteam/e-Paper/blob/master/RaspberryPi_JetsonNano/c/lib/e-Paper/EPD_7in5_V2.c);
its license is in `third_party/Waveshare-LICENSE.txt`.

For a browser preview that updates automatically while you edit:

```sh
bash scripts/preview.sh
```

This opens a local viewer at `http://127.0.0.1:8765/`. Choose morning, afternoon,
or the hardware test image. Saving source files or the configuration updates
the images; the browser updates without clicking or refocusing it.
The layout clocks also advance automatically every minute without rebuilding.
The viewer builds in `build/preview/native`, separately from `build/macos`.
Restart the preview script after updating the watcher itself.
If a build fails, the last successful image stays visible and the error is
printed in the terminal. Stop the watcher with Ctrl+C. The viewer binds only to
your Mac's loopback interface and uses Python's standard library plus the
existing CMake/Clang build and macOS `sips` image converter. Use `--no-open` to
skip opening a browser, or `--port 8766` to choose another port. `LLVM_ROOT` and
`PYTHON` work as with the build script.

To generate a single preview file instead:

```sh
cmake -S . -B build/macos -G Ninja \
  -DCMAKE_CXX_COMPILER="$(brew --prefix llvm)/bin/clang++"
cmake --build build/macos
build/macos/family-dashboard --preview build/display-preview.pbm
sips -s format png build/display-preview.pbm --out build/display-preview.png
```

## Text rendering

### Dashboard layout experiment

Generate morning and afternoon layouts at the panel's exact 800x480 resolution:

```sh
build/macos/family-dashboard --preview-morning build/morning.pbm
build/macos/family-dashboard --preview-afternoon build/afternoon.pbm
sips -s format png build/morning.pbm --out build/morning.png
sips -s format png build/afternoon.pbm --out build/afternoon.png
```

Both layouts show a 24-hour clock (HH:MM) and the current date using the
machine's local timezone. The morning agenda is labelled with today's date;
the afternoon agenda uses tomorrow's calendar date, including across month,
year, and daylight-saving boundaries. Date labels use English abbreviations.
The browser preview advances it each minute; a physical display command draws
the time at invocation and still needs scheduling for ongoing minute updates.
On the Pi, set the timezone to `Europe/Stockholm` with `sudo raspi-config` if needed.

Both layouts use your configured family summaries, dated deviations, and good
things ahead. They do not fetch external data or switch modes automatically.
To try them on the physical panel, build and upload the executable and your
configuration, then select a layout:

```sh
bash scripts/build.sh
bash scripts/deploy.sh
bash scripts/display.sh --morning
bash scripts/display.sh --afternoon
```

The display script also accepts an SSH target, for example
`bash scripts/display.sh your-user@zero.local --morning`.
The executable provides `--display-morning` and `--display-afternoon` for direct
use on the board. The existing `--preview`, `--display`, and display script
without a mode continue to use the hardware test image (`--test` selects it
explicitly).

To compare the vendor's fast refresh mode with full refresh:

```sh
bash scripts/display.sh --morning --fast
bash scripts/display.sh --afternoon --fast
bash scripts/display.sh --morning --full
```

Rebuild and upload after changing the executable. Direct board commands also
accept a trailing `--partial`, `--fast`, or `--full`, for example
`family-dashboard --display-morning --partial`. Partial refresh is the
default in the executable and display script. Fast mode follows
`EPD_7IN5_V2_Init_Fast` in the
[Waveshare reference driver](https://github.com/waveshareteam/e-Paper/blob/master/RaspberryPi_JetsonNano/c/lib/e-Paper/EPD_7in5_V2.c),
using the same image transfer and power-off/deep-sleep sequence. This is a
whole-screen fast refresh, not a partial update. It may still flash; actual
speed, contrast, and ghosting need verification on the physical panel. If
afterimages appear, use `--full` for a cleaning refresh. Refresh timing is
printed by the executable for comparison.

To experiment with an update that avoids the whole-screen flash:

```sh
bash scripts/build.sh
bash scripts/deploy.sh
bash scripts/display.sh --demo-partial
```

The demo draws morning with partial refresh, waits five seconds, then switches
to afternoon with another partial refresh. There is no initial fast or full
refresh. Watch both transitions for flashing, inverted text, or afterimages. This
uses the vendor's partial initialization and window commands, with white=1
pixel data as in its Python driver. The controller stays powered between the
two images and enters deep sleep after the demo. The direct executable option
is `--demo-partial`; it cannot be combined with layout or refresh flags in the
display script.

Normal display commands now also attempt standalone partial updates after waking
from deep sleep, using the retained visible image without restoring controller
RAM. This wake-up behavior and all refresh modes still need verification on
the Zero 2 W. Ghosting may accumulate during this experiment. Individual display
commands do not trigger a cleaning refresh automatically after an error. Use
`bash scripts/display.sh --afternoon --full` for a cleaning refresh.

### Font coverage

The built-in 5x7 font covers printable ASCII (uppercase, lowercase, numbers,
and punctuation), `ÅÄÖåäö`, `Éé`, the degree sign, euro sign, typographic quotes,
dashes, ellipsis, and bullet. Text is decoded as UTF-8 and centered by character
count. Unsupported Unicode and malformed UTF-8 use a visible `?` fallback;
this small bitmap font does not cover all Unicode or combine accent marks.

Native renderer tests cover character coverage, glyph width, UTF-8 centering,
and invalid input:

```sh
cmake --build build/macos
ctest --test-dir build/macos --output-on-failure
```

## VS Code

Install the recommended **clangd** extension (`llvm-vs-code-extensions.vscode-clangd`).
The workspace uses Homebrew clangd with experimental named-module support,
C++26, and the native macOS compilation database. Microsoft C/C++ IntelliSense
is disabled in this workspace to avoid competing module diagnostics; its debugger
remains available. The workspace also treats `.cppm` files as C++.

Generate the editor's build configuration and module artifacts once:

```sh
cmake -S . -B build/macos -G Ninja \
  -DCMAKE_CXX_COMPILER="$(brew --prefix llvm)/bin/clang++"
cmake --build build/macos
```

CMake exports `build/macos/compile_commands.json` with the compiler flags for
each source file. `.clangd` locates this database and removes CMake's module-file
arguments so clangd can build and cache its own module artifacts. Keep the imports
in `main.cpp` before the standard headers to avoid clangd preamble conflicts with
libc++ declarations from the imported modules. Rebuild after changing module imports.
The VS Code settings use the Apple Silicon Homebrew path `/opt/homebrew/opt/llvm`;
adjust the compiler and clangd paths in `.vscode` if LLVM is installed elsewhere.
The editor uses the macOS configuration; `scripts/build.sh` still cross-compiles
the Linux display backend for the board.

After switching extensions, run **Developer: Reload Window** from the Command
Palette. Use **clangd: Restart language server** if diagnostics become stale.
Named-module support in clangd is experimental; the CMake build is the final
check for compiler errors. See [clangd's module support documentation](https://clangd.llvm.org/features#experimental-c20-modules-support).

## Coding style

All C++ sources and module files use Allman braces with two-space indentation,
as defined by `.clang-format`. Opening braces go on their own lines, including
for short functions and control-flow bodies. `AGENTS.md` records this convention
for coding agents. Editors with clang-format support can use the same config.
Source files use `using namespace std;` with unqualified standard-library names;
module units place the directive after their module declaration and imports.

```sh
bash scripts/format.sh          # Format all C++ sources and modules
bash scripts/format.sh --check  # Fail if formatting differs
```

The configured CMake build also provides `format` and `format-check` targets.
Building the executable automatically runs `format-check` and fails on drift.
The script uses the system's `find` and clang-format from the LLVM installation.
Set `CLANG_FORMAT` to select a different formatter executable.

## Toolchain and compatibility

CMake and Ninja track named-module dependencies using Clang's module scanner.
`cmake/zero.cmake` targets `aarch64-linux-gnu` and Cortex-A53, uses the local
ARM64 Linux sysroot, and links with LLD. Apple Clang and the macOS linker are
not used for the Linux executable.

The ARM64 executable is statically linked, avoiding target-side glibc and
libstdc++ version dependencies. The Linux GPIO v2 API requires kernel 5.10 or
newer; the target is the Zero 2 W on 64-bit Raspberry Pi OS Trixie. Actual
execution and display output on the Pi are the final compatibility checks.

`LLVM_ROOT`, `LLD_ROOT`, and `PYTHON` override the build script's Homebrew tool
locations. Downloads and generated outputs are contained in the ignored
`build/` directory. Delete `build/sysroot-arm64` and the relevant downloads to
refresh target packages.

## Verification

Before deployment, run:

```sh
bash scripts/format.sh --check
bash scripts/build.sh
cmake -S . -B build/macos -G Ninja \
  -DCMAKE_CXX_COMPILER="$(brew --prefix llvm)/bin/clang++"
cmake --build build/macos
ctest --test-dir build/macos --output-on-failure
```

Native tests exercise configuration validation, weekly schedules, dated
overrides, countdown rendering, framebuffer and UTF-8 rendering, minute
updates, and midnight rollover. The ARM64 build compiles
the Linux SPI and GPIO code but does not exercise hardware. After deployment,
run the greeting and generate a preview on the Pi before testing the HAT. Start
with `--test --full`, then validate morning/afternoon layouts, fast refresh,
and partial refresh on the physical panel. The Zero hardware path is not yet
verified.

### Hourly cleaning refresh

The morning timer updates the display each minute, using a full refresh at
minute `00` of every hour and partial refreshes during the other minutes.
To install or update it, copy both scripts to the Pi and run:

```sh
scp scripts/install-morning-timer.sh scripts/refresh-morning.sh your-user@zero.local:~
ssh -t your-user@zero.local 'bash ~/install-morning-timer.sh'
```

The installer deploys the refresh script beside the existing dashboard binary
and restarts the timer. Full refreshes may flash the panel. Missed updates while
the Pi is off are skipped; the next full refresh runs at the next hour boundary.
