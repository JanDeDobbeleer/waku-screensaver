# WAKU Screensaver

An experimental, standalone Windows screensaver inspired by Omarchy. It animates WAKU block art
with rotating text effects. It does not need WAKU, Python, or a terminal.

The prototype cycles through twelve effects, inspired by the
[TerminalTextEffects showroom](https://chrisbuilds.github.io/terminaltexteffects/showroom/):
rain, decrypt, waves, beams, matrix, burn, falling blocks, a black hole, a perspective starfield,
retro drifting stars, parallax twinkles, and an original arcade chase. They are native C++
interpretations, not exact ports of the original effects. Falling blocks can cross the background;
letters and numbers appear only within the WAKU lettering.

## Build

Requires Visual Studio 2022 C++ tools, the Windows SDK, and CMake:

```powershell
cmake -S . -B build
cmake --build build --config Release
ctest --test-dir build -C Release --output-on-failure
```

The result is `build\Release\waku-screensaver.scr`, or `build\waku-screensaver.scr` with a
single-configuration generator such as Ninja. A build must target the intended Windows architecture.

Run `pwsh -NoProfile -File .\tools\check.ps1 -Architecture arm64` to check formatting, build, and run
tests. Use `-Architecture x64` for an x64 build. This requires Ninja and clang-format as well as the
C++ tools. These builds go into `build\<architecture>-release`.

### C++ static analysis

Run MSVC `/analyze` and clang-tidy locally with:

```powershell
pwsh -NoProfile -File .\tools\check.ps1 -Architecture arm64 -Analyze
```

Use `-Architecture x64` for x64 analysis. This requires LLVM's clang-tidy in addition to the
normal build tools. The script checks formatting, then analyzes all production C++ sources
and their project headers. `.clang-tidy` defines focused correctness, performance, and naming
checks; findings fail the command.

Analysis uses a clean, isolated `build\<architecture>-release-analyze` directory and writes
`msvc-analyze.log` and `clang-tidy.log` there. It does not replace the ordinary build and tests.
CI runs the same analysis command for x64 alongside the x64 and ARM64 build/test jobs and
uploads analysis logs.

## Download and release

Download the package for your Windows architecture from
[Releases](https://github.com/JanDeDobbeleer/waku-screensaver/releases). Release ZIPs contain the
signed `.scr`, the identical signed `.exe`, this README, and the MIT license. Ordinary CI artifacts
are unsigned.

Maintainers must configure the repository secrets `AZURE_CLIENT_ID`, `AZURE_CLIENT_SECRET`, and
`AZURE_TENANT_ID`. The release workflow uses the same Azure Trusted Signing account and certificate
profile as WAKU. Signing is mandatory for releases and fails if credentials are missing or signature
verification fails.

To publish, push a `v<major>.<minor>.<patch>` tag. Alternatively, run the **release** workflow from
`main`, enter a version without `v`, and enable **publish**. Leave **publish** unchecked to build
signed artifacts without creating a release.

## Try it

Run the accompanying `waku-screensaver.exe --demo` for a windowed demo. Run
`waku-screensaver.exe /s` for the full-screen screensaver; a key press, mouse click, or mouse
movement exits it. The `.exe` and `.scr` are the same program; Windows may ignore command-line
arguments passed to a `.scr` through its shell association.

To start the demo with a particular effect, use `waku-screensaver.exe --demo --effect=chase`.
Other effect names are `rain`, `decrypt`, `waves`, `beams`, `matrix`, `burn`, `blocks`, `blackhole`,
`starfield`, `retro-stars`, and `parallax`. The demo continues cycling through effects after the
selected one.

To install, keep the `.scr` in a permanent location, right-click it, and select **Install**. In
Windows Screen Saver Settings, choose the idle time and, if desired, **On resume, display logon
screen**. The settings dialog also provides a preview.

The screensaver is not a lock screen. Windows controls sign-in on resume and display sleep.

## Configuration

The screensaver reads `%USERPROFILE%\.config\waku\screensaver.ini` once when it starts,
including in demo and preview modes. Click **Settings** in Windows Screen Saver Settings
(or run `waku-screensaver.exe /c`) to create the file with defaults and open it in Notepad:

```ini
[screensaver]
word=WAKU
animations=rain,decrypt,waves,beams,matrix,burn,blocks,blackhole,starfield,retro-stars,parallax,chase
```

`word` accepts up to 32 ASCII letters, digits, and spaces. Lowercase letters become uppercase.
`WAKU` keeps its original artwork; other words use a seven-row block font and scale to fit.

`animations` is a comma-separated list of the effect names above. Only listed effects run,
in the specified order. Effects last nine seconds except `chase`, which lasts 28 seconds for WAKU
and scales up for words wider than WAKU. A single effect repeats on its own;
duplicate names are ignored. Names and keys are case-sensitive. `--demo --effect=blocks`
starts with that effect, temporarily adding it if it is not enabled in the file.
`blackhole` pulls stars inward from across the screen before the lettering reforms;
`starfield` sends perspective streaks out to the screen edges. Both keep the word centered.
`retro-stars` drifts layered stars across the word, and `parallax` twinkles stars within the letters.
`chase` traces the lit pixels at a distance-based speed with two particle runners: one eats centered
circular dots that shrink into it, the other steadily closes the gap and catches it at the final dot.
A collision burst then triggers a wave that restores the word.

Missing files or settings use the existing defaults. Invalid values leave that setting at its
previous valid value (or its default); an empty animation list is invalid. Unknown keys and
sections are ignored. Use UTF-8, with optional BOM; lines starting with `;` or `#` are comments.
Files larger than 16 KiB are ignored. Save changes and restart the screensaver to apply them.
No Waku installation is required.

## Code

The animation module computes frames without Windows dependencies. Each effect has its own source
file. The Windows application owns window lifecycle and uses a separate Direct2D renderer. Effects
that have settled pause redraws until the next transition.
