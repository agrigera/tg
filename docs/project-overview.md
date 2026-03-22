# Tg Overview

## What This Software Does

Tg is a desktop application for measuring the performance of a mechanical watch.

In practical terms, you place a watch near a microphone, let the program listen to the ticking sound, and Tg estimates values that watchmakers and enthusiasts care about, including:

- rate or daily deviation
- beat error
- amplitude
- beat frequency
- calibration-related measurements

The application behaves like a software timegrapher. Instead of using dedicated hardware, it uses the computer's audio input and signal processing to analyze the sound coming from the movement.

## Who It Is For

- watchmakers
- hobbyists who regulate or diagnose mechanical watches
- developers interested in audio-based measurement tools

## Supported Operating Systems

According to the repository documentation, Tg is known to work on:

- Windows
- macOS
- Linux

The README also states that it should be possible to build it on other modern Unix-like systems.

## Technical Requirements

To use Tg in practice, you need more than just the binary.

### Hardware

- a microphone or other working audio input device
- a machine with audio capture enabled and correctly configured
- ideally a quiet environment, or a way to place the watch very close to the mic

Without a usable audio input, the software cannot analyze the watch.

### Software Runtime Requirements

At build and link time, the project depends on:

- GTK+ 3 and GLib 2.0 for the desktop UI
- PortAudio 2.0 for audio input
- FFTW3 single-precision (`fftw3f`) for signal processing
- pthread for threading
- libm for math functions

### Build Toolchain Requirements

The current build system is based on Autotools and a C compiler.

Typical requirements are:

- a C99-capable compiler, with GCC and Clang explicitly mentioned in the repo
- `autoconf`
- `automake`
- `libtool`
- `make`
- `pkg-config`

On Windows, the README suggests MSYS2 for the build environment.

## Dependency Status

### Dependencies used by the codebase

- GTK+ 3
- GLib 2.0
- PortAudio 2.0
- FFTW3
- pthread
- libm
- Autotools-based build tooling

### Are they outdated?

Short answer: some parts of the stack are old, but not all of them are obsolete.

- GTK+ 3 is a mature generation of GTK and older than GTK 4. It is still widely available, but it is no longer the newest UI stack.
- PortAudio and FFTW are long-standing libraries. They are conservative choices, not trendy ones, and that is not necessarily a problem for this kind of application.
- Autotools is also an older build approach compared with newer systems like CMake or Meson, but it remains valid if the project is maintained.

What does look outdated in this repository is not the core DSP stack itself, but the surrounding project metadata and documentation:

- the README still references an old repository location
- the README still references a Travis CI badge
- some external URLs look historical and were not validated in this review
- the README still says there is no manual, while the repository already contains a man page

### Practical conclusion

The codebase uses a conservative native desktop stack. That does not automatically make it wrong or unusable. The bigger maintenance concern today is stale documentation and project metadata, not that the application depends on inherently broken libraries.

## Current Limitations Of This Assessment

This overview is based on the source tree and documentation currently in the repository.

It does not confirm:

- that all listed binary download links still work
- that all supported operating systems still build successfully today
- that every dependency version combination is currently tested in CI

During the review, the local Windows environment did not have the full native build toolchain installed, so a full build verification was not performed.

## Related Files

- `README.md`
- `docs/microphone-and-calibration-guide.md`
- `configure.ac`
- `Makefile.am`
- `docs/tg-timer.1`
