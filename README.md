# Family dashboard

C++26 family dashboard using named modules, cross-compiled directly on macOS
for the Tinkerboard S (ARMv7 hard-float). No container or VM.

## Build and upload

Install the native build tools with Homebrew:

```sh
brew install llvm lld cmake ninja python@3.14
bash scripts/build.sh
bash scripts/deploy.sh
```

The build script uses Homebrew Clang and LLD to produce a static ARM Linux
executable at `build/armv7-native/family-dashboard`. On first run it downloads
Debian trixie ARM headers and libraries into `build/sysroot`. Packages are
SHA256-checked against the Debian package index fetched over HTTPS; package
names, versions, URLs, and hashes are recorded in `build/sysroot/packages.json`.
These target packages are extracted locally, never installed on macOS.

The deploy script uses its configured SSH destination, uploads the executable to
`~/.local/bin/family-dashboard-hello`, and runs it. Expected output:

```text
hello world
```

An alternative SSH destination can be supplied as the first argument:

```sh
bash scripts/deploy.sh linaro@tinkerboard.local
```

SSH uses your normal keys or interactive password prompt. Passwords are not
stored in this repository.

## Display test

The display test targets the Waveshare 7.5-inch **black-and-white V2, 800x480**,
with its Driver HAT plugged directly into the Tinkerboard's 40-pin header.
It uses the newer V2 full-refresh initialization and OTP waveform, not the
separate legacy V2 software-waveform driver or the color/HD panel drivers.

```sh
bash scripts/build.sh
bash scripts/deploy.sh
bash scripts/display.sh
```

The first two commands build, upload, and print `hello world`. The third sends
a test image with a border, `FAMILY DASHBOARD`, `HELLO WORLD`, and a checker
pattern to the panel, waits for refresh completion, and puts it into deep sleep.
The image remains visible after power-down. Refreshes flash the screen.

`display.sh` requires TinkerOS's `gpio` utility and passwordless `sudo` (as on
this board). It temporarily switches WiringPi pins 0, 5, 6, and 1 to GPIO,
including pins assigned to UART4 and PCM/I2S at boot. Those header functions
cannot be used simultaneously with this HAT. No boot configuration is edited.
The executable exports GPIOs through sysfs, refuses existing exports, and
unexports its pins on normal completion or C++ exceptions. Output pins are
driven low when released. BUSY waits time out after 30 seconds.

| Signal | Physical header pin | Linux GPIO / SPI |
| --- | --- | --- |
| MOSI | 19 | SPI2 MOSI |
| CLK | 23 | SPI2 SCLK |
| CS | 24 | SPI2 CS0 (`/dev/spidev2.0`) |
| RST | 11 | 164 |
| DC | 22 | 171 |
| BUSY | 18 | 163 |
| PWR (newer HATs) | 12 | 184 |

SPI mode 0, 8-bit words, 500 kHz, at most 32 bytes per full-duplex transfer
(received data is discarded). Larger TX-only transfers produced missing text
and barcode-like stripes on this board's old Rockchip driver; the smaller
full-duplex transfers produced the correct full image. No external GPIO
library is linked into the executable. Register settings are adapted from
[Waveshare's reference driver](https://github.com/waveshareteam/e-Paper/blob/master/RaspberryPi_JetsonNano/c/lib/e-Paper/EPD_7in5_V2.c);
its license is in `third_party/Waveshare-LICENSE.txt`.

To preview the exact framebuffer on macOS without accessing GPIO:

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

These previews use fixed fictional sample data: today's timeline and weather
in the morning, tomorrow's timeline in the afternoon, plus countdowns,
household reminders, and shopping in both. They do not fetch live data or
switch modes automatically. To try them on the physical panel, build and upload
the executable, then select a layout:

```sh
bash scripts/build.sh
bash scripts/deploy.sh
bash scripts/display.sh --morning
bash scripts/display.sh --afternoon
```

The display script also accepts an SSH target, for example
`bash scripts/display.sh linaro@tinkerboard.local --morning`.
The executable provides `--display-morning` and `--display-afternoon` for direct
use on the board. The existing `--preview`, `--display`, and display script
without a mode continue to use the hardware test image (`--test` selects it
explicitly).

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
The toolchain sets `--target=arm-linux-gnueabihf`, the local Linux sysroot, and
LLD for ELF linking. Apple Clang and the macOS linker are not used.

For this smoke test, the executable is statically linked, avoiding a dependency
on the board's installed glibc or libstdc++ versions. ARMv7 hard-float and a
Linux kernel compatible with the toolchain are still required. Successful
execution on the board is the final compatibility check.

Before adding networking, graphics, and GPIO dependencies, inspect the board:

```sh
cat /etc/os-release
uname -m
getconf GNU_LIBC_VERSION
dpkg --print-architecture
```

The inspected board runs Debian 9 (stretch), kernel 4.4.132, with SPI2 enabled.
Choose a target sysroot and runtime deployment strategy for future dynamic
dependencies; this display test is still a standalone static executable.

`LLVM_ROOT`, `LLD_ROOT`, and `PYTHON` override the build script's Homebrew tool
locations. Downloads and generated outputs are contained in the ignored
`build/` directory. Delete that directory to refresh all target packages.

## Verification

The native macOS cross-build passed with Clang/LLD 23.1.4, CMake 4.4.4, and
Ninja 1.13.2. ELF inspection confirms ARMv7, hard-float ABI, static linking,
and a Linux 3.2 minimum kernel note. Script syntax checks passed.

The binary was uploaded over SSH and prints `hello world` on the board.
Both ARM and macOS builds pass, and the generated preview was visually checked.
The ARM and macOS preview files have identical SHA-256 hashes. The display
refresh takes about 3.6 seconds, followed by power-off and deep sleep. The user
confirmed the complete test image is correct on the physical panel after the
SPI transfer fix. All GPIO exports owned by the application were released.
