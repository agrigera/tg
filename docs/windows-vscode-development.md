# Windows VS Code Development Setup

## Purpose

This document defines the intended development workflow for building and running Tg from Visual Studio Code on Windows.

It is the first practical milestone in the current action plan because the project cannot be modernized safely until it can be compiled and run locally with low friction.

## Initial Machine Findings

The Windows environment inspected on 2026-03-22 had these characteristics:

- `git` was available
- `winget` was available
- `gcc` was not available
- `make` was not available
- `sh` was not available
- `pkg-config` was not available
- `autoconf` was not available
- `automake` was not available
- `libtool` was not available
- `C:\msys64` was not present

At that point, the blocker was not source code. The blocker was the missing native build environment.

## Current Status

After provisioning and verification on 2026-03-22:

- MSYS2 is installed in `C:\msys64`
- the required MinGW64 packages for Tg are installed
- `sh autogen.sh`, `./configure`, and `make -j4` complete successfully
- `tg-timer.exe` launches successfully from the MinGW64 environment
- `.vscode/tasks.json` has been added for `configure`, `build`, `rebuild`, and `run`
- `gdb` is not installed yet, so a debugger launch configuration has not been added yet

## Recommended Toolchain

Use MSYS2 with the MinGW-w64 64-bit toolchain.

This matches the project's historical Windows guidance and is the least risky path for getting the existing Autotools build working.

## Step 1 - Install MSYS2

MSYS2 can be installed via `winget`.

Package identifier found during review:

- `MSYS2.MSYS2`

Suggested install command from PowerShell:

```powershell
winget install --id MSYS2.MSYS2 --exact --accept-source-agreements --accept-package-agreements
```

Expected default install location:

- `C:\msys64`

## Step 2 - Install Build Dependencies In MSYS2

Open the `MSYS2 MinGW x64` shell and install the required packages.

Suggested package set:

```sh
pacman -S --needed \
  mingw-w64-x86_64-gcc \
  mingw-w64-x86_64-gtk3 \
  mingw-w64-x86_64-portaudio \
  mingw-w64-x86_64-fftw \
  make \
  pkg-config \
  git \
  autoconf \
  automake \
  libtool
```

If `bash` and standard MSYS tools are installed through the base environment, `sh` and related tooling should then be available.

## Step 3 - Verify The Build Manually

From the same shell:

```sh
cd /t/Proyectos/tg
./autogen.sh
./configure
make
```

If that works, the project has a known-good Windows baseline.

## Step 4 - Move The Workflow Into VS Code

Once the manual build works, add:

- `.vscode/tasks.json` for configure, build, and run
- `.vscode/launch.json` if debugger integration is practical

Current repository status:

- `.vscode/tasks.json` has been added for `configure`, `build`, `rebuild`, and `run`
- `gdb` is not installed yet, so a debugger launch configuration has not been added yet

The most likely implementation is to invoke MSYS2 bash explicitly from VS Code, for example through:

- `C:\msys64\usr\bin\bash.exe`

with commands executed in the repository root.

## Recommended VS Code Workflow

The intended development loop is:

1. run configure task
2. run build task
3. run launch or run task
4. iterate on code and UI with a repeatable local loop

## Runtime Audio Controls (Current Behavior)

The current UI now includes two runtime controls in the top bar:

- `audio`: selects the PortAudio input device
- `rate`: selects the requested input sample rate

When either control changes:

- Tg restarts PortAudio input
- Tg restarts the compute pipeline
- the selected values are persisted in config (`audio_device`, `audio_rate`)

If the selected rate is unsupported by the chosen device, the application falls back to `44.1 kHz` (`44100`) and continues running.

UX notes implemented in this repository state:

- the `audio` list marks the system default device with `(default)`
- the `rate` list shows human-readable labels (`22.05 kHz`, `44.1 kHz`, etc.)

## Build A Windows MSI Installer

The repository already includes a WiX-based MSI pipeline in `packaging/`.

### Prerequisites

- WiX Toolset v3 commands available in PATH: `heat`, `candle`, `light`
- `php` and `uuidgen` available in the same shell (used to generate `packaging/tg-timer.wxs` from template)
- MSYS2/MinGW64 build toolchain already working (`./configure` + `make`)
- a `resources` directory with runtime files to be bundled in the installer (for example GTK/PortAudio/FFTW runtime DLLs and related resources)

### Command

From the repository root in an MSYS2 shell:

```sh
./packaging/make_msi.sh /path/to/resources
```

This now performs the full flow:

1. regenerates `packaging/tg-timer.wxs` from the template
2. builds `tg-timer.exe`
3. stages installer payload into `msi/`
4. compiles and links WiX sources
5. writes `msi/tg-timer_<version>.msi`

If a required tool is missing, the script exits early with a specific error message.

## What Not To Do Yet

Do not start with these tasks before the Windows build is working:

- migrate the codebase to C++
- replace Autotools outright
- redesign the UI in a new toolkit
- attempt broad architectural refactors

Those changes should come only after the current build can be reproduced and debugged locally.

## Immediate Next Action

The build-and-run milestone is in place and the core UI backports for audio/rate control are integrated.

The next practical steps are:

1. add debugger support if Windows debugging inside VS Code is needed
2. run targeted runtime checks using different microphones and rates
3. continue backlog work (documentation refinement and remaining algorithm backports)