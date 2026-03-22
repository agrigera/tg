# Tg

Tg is a desktop timegrapher for mechanical watches. It listens to the ticking sound through the machine's audio input and estimates values such as rate, beat error, amplitude, and beat frequency.

## Download

- Windows installer (.msi): https://github.com/agrigera/tg/releases/download/v0.8.0/tg-timer_0.8.0.msi
- Latest release page (all assets): https://github.com/agrigera/tg/releases/latest

## What You Need

- a working microphone or audio input device
- a quiet enough environment to capture the watch clearly
- the correct BPH and lift angle if you want the most reliable readings
- calibration, or at least a known-good setup, if you care about accuracy in seconds per day

If you want practical setup advice instead of just build instructions, start here:

- [docs/project-overview.md](docs/project-overview.md)
- [docs/microphone-and-calibration-guide.md](docs/microphone-and-calibration-guide.md)
- [docs/windows-vscode-development.md](docs/windows-vscode-development.md)

Recent builds include top bar `audio` and `rate` controls (input device and sample rate). See the microphone guide for practical usage and troubleshooting.

## Supported Platforms

The project documentation and packaging history indicate support for:

- Windows
- macOS
- Linux

The codebase is a native GTK application and should also be portable to other Unix-like systems with the required dependencies.

## Dependencies

Build and runtime depend on:

- GTK+ 3
- GLib 2.0
- PortAudio 2.0
- FFTW3 single-precision (`fftw3f`)
- pthread
- libm

Build tooling depends on:

- a C99-capable compiler
- `autoconf`
- `automake`
- `libtool`
- `make`
- `pkg-config`

## Build From Source

Generic release build:

```sh
git clone https://github.com/agrigera/tg.git
cd tg
./autogen.sh
./configure
make
```

Debug build:

```sh
make tg-timer-dbg
```

## Platform Notes

### Windows

The historical build path is MSYS2 with MinGW-w64.

For the intended developer workflow inside VS Code on Windows, see [docs/windows-vscode-development.md](docs/windows-vscode-development.md).

Typical package set:

```sh
pacman -S mingw-w64-x86_64-gcc make pkg-config mingw-w64-x86_64-gtk3 mingw-w64-x86_64-portaudio mingw-w64-x86_64-fftw git autoconf automake libtool
```

Then build with the generic steps above.

### Debian and Ubuntu

```sh
sudo apt-get install libgtk-3-dev libjack-jackd2-dev portaudio19-dev libfftw3-dev git autoconf automake libtool
git clone https://github.com/agrigera/tg.git
cd tg
./autogen.sh
./configure
make
```

`libjack-jackd2-dev` is included in the original instructions as a workaround for an old Debian bug.

### Fedora

```sh
sudo dnf install fftw-devel portaudio-devel gtk3-devel autoconf automake libtool
git clone https://github.com/agrigera/tg.git
cd tg
./autogen.sh
./configure
make
```

### macOS

This repository does not currently provide a verified, maintained macOS installation path in-tree.

If you work on macOS, expect to verify your GTK, PortAudio, and microphone permission setup manually.

## Before You Trust The Numbers

Check these first:

1. The microphone is actually capturing the watch clearly.
2. The watch is positioned very close to the microphone.
3. The room is quiet enough for stable measurements.
4. The selected BPH is correct, or the guessed value is plausible.
5. The lift angle is correct if amplitude matters.
6. Calibration has been performed or at least checked for repeatability.

## Project Status

The software is useful and still worth maintaining, but the repository has some stale metadata and documentation that are being cleaned up.

Known maintenance realities today:

- external discussions about Tg are generally positive about the tool itself
- users repeatedly ask for better microphone and calibration guidance
- packaging and maintenance visibility are spread across multiple forks and downstream packages

Repository-tracked review notes live under [docs/issues](docs/issues).

Recent implementation progress is tracked in [docs/issues/backport-progress-2026-03-22.md](docs/issues/backport-progress-2026-03-22.md).

## License

Tg is distributed under the GNU GPL version 2. See [LICENSE](LICENSE).
