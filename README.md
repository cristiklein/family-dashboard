# Family dashboard

C++26 family dashboard using named modules, cross-compiled directly on macOS
for Raspberry Pi Zero 2 W running 64-bit Raspberry Pi OS (Debian Trixie).
No container, VM, or compiler installation on the Pi is needed.

## Build and upload

Install the native build tools with Homebrew:

```sh
brew install llvm lld cmake ninja python@3.14
./scripts/build.sh
./scripts/deploy.sh
```

The deploy script defaults to `cklein@zero.local` and installs the executable
at `~/.local/bin/family-dashboard` on the Pi. To deploy to another SSH destination:

```sh
./scripts/deploy.sh your-user@zero.local
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
  IDs and high-level summaries. Blank or omitted entries show `No plans`.
- **Deviations:** `deviations` maps exact dates to member summaries that replace
  their usual plans for that day. Everyone else keeps their weekly schedule.
  Leave a summary blank to show `No plans` for that date. Changed people have a
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
and summary values written as lists or mappings are rejected. Keep summaries
short so they fit in the panel's two lines before it adds `...`.

The executable reads `dashboard.yaml` in the current directory, falling back to
`~/.config/family-dashboard/dashboard.yaml` if no local file exists. Override
this with `--config /path/to/plans.yaml` on any dashboard command. Missing or
invalid configuration produces an error before touching the display.

Create the configuration directory once, then copy your plans to the Pi:

```sh
ssh your-user@zero.local 'install -d -m 700 ~/.config/family-dashboard'
scp dashboard.yaml your-user@zero.local:.config/family-dashboard/
```

After editing your plans, rerun the `scp` command. The dashboard reads the file
on each refresh.

## Display test

Use the Waveshare 7.5-inch **black-and-white V2 (800×480)** display with its
Driver HAT. Follow the Raspberry Pi connection instructions in the
[Waveshare HAT manual](https://www.waveshare.com/wiki/7.5inch_e-Paper_HAT_Manual).

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

The `spi` and `gpio` groups let your account access the display. The binary
reports the device and suggests a fix if access fails.

After deploying, run this on the Pi for the first hardware test:

```sh
~/.local/bin/family-dashboard --test --full
```

This sends a test image with a border, greeting, and checker pattern to the panel,
waits for completion, then powers down and enters deep sleep. The image remains
visible. Full and fast refresh may flash the screen.

## Text rendering

### Dashboard layouts

Both layouts show a 24-hour clock (HH:MM) and the current date using the
machine's local timezone. The morning agenda is labelled with today's date;
the afternoon agenda uses tomorrow's calendar date, including across month,
year, and daylight-saving boundaries. Date labels use English abbreviations.
Each refresh draws the time at invocation. Use the systemd timer for ongoing
minute updates.
On the Pi, set the timezone to `Europe/Stockholm` with `sudo raspi-config` if needed.

Both layouts use your configured family summaries, dated deviations, and good
things ahead. Try them on the Pi after deploying the executable and setting up
the YAML file:

```sh
~/.local/bin/family-dashboard --help
~/.local/bin/family-dashboard
~/.local/bin/family-dashboard --morning
~/.local/bin/family-dashboard --afternoon --fast
```

Run `family-dashboard --help` for supported options and the default layout and
refresh schedule. The binary logs progress to stdout and errors to stderr.

Fast mode follows
`EPD_7IN5_V2_Init_Fast` in the
[Waveshare reference driver](https://github.com/waveshareteam/e-Paper/blob/master/RaspberryPi_JetsonNano/c/lib/e-Paper/EPD_7in5_V2.c),
using the same image transfer and power-off/deep-sleep sequence. This is a
whole-screen fast refresh, not a partial update. It may still flash; actual
speed, contrast, and ghosting need verification on the physical panel. If
afterimages appear, use `--full` for a cleaning refresh. Refresh timing is
printed by the executable for comparison.

Normal display commands now also attempt standalone partial updates after waking
from deep sleep, using the retained visible image without restoring controller
RAM. This wake-up behavior and all refresh modes still need verification on
the Zero 2 W. Ghosting may accumulate during this experiment. Individual display
commands do not trigger a cleaning refresh automatically after an error. Use
`~/.local/bin/family-dashboard --afternoon --full` on the Pi for a cleaning
refresh.

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

Install [pre-commit](https://pre-commit.com/#installation) and enable the hook
once per clone:

```sh
brew install pre-commit
pre-commit install
```

Clang-format applies `.clang-format` to staged C++ files. If it makes changes,
stage the fixes and retry the commit. Yamllint checks YAML syntax, duplicate
keys, and two-space indentation. The CLI and the hook both read `.yamllint`.
ShellCheck checks shell scripts.

## Verification

Before deployment, run:

```sh
./scripts/build.sh
cmake -S . -B build/macos -G Ninja \
  -DCMAKE_CXX_COMPILER="$(brew --prefix llvm)/bin/clang++"
cmake --build build/macos
ctest --test-dir build/macos --output-on-failure
```

Native tests exercise configuration validation, weekly schedules, dated
overrides, countdown rendering, framebuffer and UTF-8 rendering, minute
updates, and midnight rollover. The ARM64 build compiles
the Linux SPI and GPIO code but does not exercise hardware. After deployment,
run `--help` on the Pi before testing the HAT. Start
with `--test --full`, then validate morning/afternoon layouts, fast refresh,
and partial refresh on the physical panel. The Zero hardware path is not yet
verified.

## Schedule updates with systemd

If you used the previous system-wide installer, disable its timer with
`sudo systemctl disable --now family-dashboard.timer`.

Once a manual refresh works, run these commands on the Pi as the same user:

```sh
mkdir -p ~/.config/systemd/user
~/.local/bin/family-dashboard --systemd-service > ~/.config/systemd/user/family-dashboard.service
~/.local/bin/family-dashboard --systemd-timer > ~/.config/systemd/user/family-dashboard.timer
systemctl --user daemon-reload
systemctl --user enable --now family-dashboard.timer
```

The timer refreshes the display every minute. To start it at boot and keep it
running after logout, run
`sudo loginctl enable-linger "$USER"`.

The service runs as your Pi account with its device groups. It uses the Pi's
local timezone and reads `~/.config/family-dashboard/dashboard.yaml` by default.
Pass layout and refresh flags to `--systemd-service` to keep those settings in
scheduled runs. Pass `--config PATH` to use another YAML file.
Reboot after changing SPI or GPIO group membership so the user service manager
picks up the new groups.

```sh
journalctl --user -u family-dashboard.service -f
systemctl --user list-timers family-dashboard.timer
systemctl --user disable --now family-dashboard.timer
```

If the Pi uses a volatile journal, read the user service's logs from the system
journal with `sudo journalctl _SYSTEMD_USER_UNIT=family-dashboard.service _UID="$(id -u)" -f`.

## Advanced Topics

### Custom systemd configuration directory

If your systemd user manager uses `XDG_CONFIG_HOME`, put both unit files in
`$XDG_CONFIG_HOME/systemd/user/`.

### Toolchain and compatibility

The build script uses Homebrew Clang and LLD to produce a static ARM64 Linux
executable at `build/arm64-native/family-dashboard`. On the first run, it
downloads Debian Trixie ARM64 packages and extracts the target headers and
libraries into `build/sysroot-arm64` for cross-compilation. The script verifies
package SHA-256 hashes against the Debian package index fetched over HTTPS and
records package names, versions, URLs, and hashes in
`build/sysroot-arm64/packages.json`.

CMake and Ninja track named-module dependencies using Clang's module scanner.
`cmake/zero.cmake` targets `aarch64-linux-gnu` and Cortex-A53, uses the local
ARM64 Linux sysroot, and links with LLD. Apple Clang and the macOS linker are
not used for the Linux executable.

Static linking avoids target-side glibc and libstdc++ version dependencies.
The Linux GPIO v2 API requires kernel 5.10 or newer. The target is the
Zero 2 W on 64-bit Raspberry Pi OS Trixie. Actual execution and display output
on the Pi are the final compatibility checks.

`LLVM_ROOT`, `LLD_ROOT`, and `PYTHON` override the build script's Homebrew tool
locations. Downloads and generated outputs are contained in the ignored
`build/` directory. Delete `build/sysroot-arm64` and the relevant downloads to
refresh target packages.

The build downloads yaml-cpp 0.8.0, verifies its SHA-256 hash, and links it
statically into the macOS and Pi builds. CMake caches the source in each build
directory. If `build/dependencies/yaml-cpp-0.8.0.tar.gz` is present, CMake uses
that archive and verifies it against the same hash.
[yaml-cpp 0.8.0 source](https://github.com/jbeder/yaml-cpp/tree/0.8.0)
