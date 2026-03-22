# Project Status - 2026-03-22

## Repository Snapshot

- Remote `origin` points to `https://github.com/agrigera/tg`.
- Local branch `master` is in sync with `origin/master`.
- Working tree was clean at review time.

## Review Scope

- Top-level documentation and build metadata.
- Build files: `configure.ac`, `Makefile.am`.
- Core source files under `src/`.
- Serialization, audio, compute, UI, and config modules.

## What Was Verified

- VS Code diagnostics reported no editor-detected errors for the workspace.
- Source code review found several concrete risks and one confirmed functional bug.

## What Was Not Verified

- Full build and runtime execution.
- GTK, PortAudio, and FFTW integration in a working toolchain.
- Packaging scripts and generated installers.

The current Windows environment used for review did not have `gcc`, `make`, `sh`, `autoconf`, `automake`, or `pkg-config` available, so build validation could not be completed here.

## Confirmed Findings

### 1. Empty snapshot files cannot be loaded back

- Priority: P1
- Type: bug
- Files: `src/serializer.c`
- Summary: `write_file()` allows writing a snapshot list with zero entries, but `read_file()` treats that successful parse as an error path.
- Evidence: end-of-file handling depends on `*s` being non-null after parsing.
- Consequence: a valid empty file is not round-trippable.

### 2. Windows double serialization uses undefined behavior

- Priority: P2
- Type: technical-debt
- Files: `src/serializer.c`
- Summary: the Windows-specific serializer reinterprets a `double` as `uint64_t` via pointer cast.
- Evidence: `uint64_t bd = *(uint64_t *)&d;`
- Consequence: this depends on strict-aliasing and ABI details and may break under optimization.

### 3. `start_computer()` leaks allocated resources on initialization failure

- Priority: P2
- Type: technical-debt
- Files: `src/computer.c`
- Summary: if thread, mutex, or condition initialization fails, the function returns `NULL` without freeing already allocated buffers and state.
- Consequence: error-path cleanup is incomplete.

### 4. Project metadata still points to the old repository

- Priority: P2
- Type: documentation
- Files: `README.md`, `configure.ac`
- Summary: the codebase now tracks `agrigera/tg`, but docs and package metadata still reference `vacaboja/tg`.
- Consequence: clone instructions, project URLs, and badges are stale.

### 5. README is outdated relative to the repository contents

- Priority: P3
- Type: documentation
- Files: `README.md`, `docs/tg-timer.1`
- Summary: README says there is still no manual, but a man page exists. It also references Travis CI and external download locations that may no longer be maintained.
- Consequence: onboarding and build guidance are less trustworthy than they should be.

### 6. Man page contains a typo

- Priority: P3
- Type: documentation
- Files: `docs/tg-timer.1`
- Summary: "mechanincal" should be "mechanical".

## Recommended Next Pass

1. Open the P1 bug for snapshot round-trip failure.
2. Open a technical-debt issue for serializer portability.
3. Open a technical-debt issue for `start_computer()` cleanup.
4. Open a documentation issue to update repository links and build instructions.
