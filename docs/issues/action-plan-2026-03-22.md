# Action Plan - 2026-03-22

## Goal

Move Tg from a documented but hard-to-verify state into a development workflow where the application can be built, run, debugged, and improved from VS Code on Windows with low friction.

## Guiding Principle

Do not start with large migrations.

The first priority is a reproducible build and run loop. After that, fix confirmed reliability problems. Only then start structural modernization such as CMake, UI refreshes, or language migration discussions.

## Recommended Order

1. Make the project build and run from VS Code on Windows.
2. Stabilize the existing code by fixing confirmed bugs and unsafe code paths.
3. Add minimal test coverage for the most fragile areas.
4. Open and track modernization work as focused issues.
5. Introduce CMake in parallel with Autotools.
6. Improve the current GTK interface incrementally.
7. Re-evaluate whether any C++ adoption is justified.

## Phase 1 - Windows Build And Run In VS Code

### Objective

Get to a state where the project can be configured, built, and launched from inside VS Code on Windows.

### Why This Comes First

- Without a working local loop, every other change is higher risk.
- UI changes are difficult to judge without running the app.
- Build system modernization is harder to validate if the baseline build is not already understood.

### Deliverables

- Verified MSYS2 and MinGW-w64 development environment.
- Working dependency installation instructions for GTK3, PortAudio, FFTW, Autotools, and related tools.
- VS Code tasks for configure, build, and run.
- Optional VS Code launch configuration for debugging.
- A repository document describing the Windows developer workflow.

### Concrete Work

1. Validate availability of `sh`, `make`, `gcc`, `pkg-config`, `autoconf`, `automake`, and `libtool`.
2. Install or document the required MSYS2 packages.
3. Verify `./autogen.sh`, `./configure`, and `make` under the intended shell.
4. Decide whether VS Code should invoke MSYS2 directly or wrap commands through PowerShell.
5. Add `.vscode/tasks.json` for build commands.
6. Add `.vscode/launch.json` if native debugging is practical.
7. Document the exact entrypoint used to launch the application on Windows.

## Phase 2 - Stabilize Confirmed Reliability Problems

### Objective

Fix known defects that undermine trust in the current code before attempting deeper modernization.

### Priority Items

1. Empty snapshot round-trip failure in `src/serializer.c`.
2. Undefined-behavior `double` serialization path on Windows in `src/serializer.c`.
3. Incomplete cleanup on initialization failure in `src/computer.c`.

### Why This Comes Before Modernization

- These are known issues with concrete technical evidence.
- They are smaller and safer than a large refactor.
- They improve confidence in later build, test, and UI work.

## Phase 3 - Add Minimal Tests

### Objective

Protect the most failure-prone areas before changing build tooling or architecture.

### Recommended Initial Coverage

- serializer round-trip tests
- empty snapshot input tests
- serialization compatibility checks for Windows-specific code paths
- small smoke-level checks for startup and failure cleanup paths where feasible

### Notes

The project currently does not appear to have an established automated test suite. The first test work should stay narrow and pragmatic.

## Phase 4 - Track Modernization As Focused Issues

### Objective

Avoid a vague "modernize everything" effort by splitting work into tracks with clear scope.

### Recommended Tracks

#### Build Track

- reproducible Windows build in VS Code
- developer environment documentation
- debugger and run workflow

#### Reliability Track

- serializer fixes
- cleanup and error-path correctness
- targeted regression coverage

#### Build System Track

- evaluate CMake alongside Autotools
- compare dependency resolution across platforms
- assess packaging impact before switching defaults

#### UI And UX Track

- review current GTK layout and information density
- improve calibration and setup discoverability
- make the interface easier to read during live use

#### Architecture Track

- evaluate where stronger abstractions are needed
- decide whether any module would materially benefit from C++

## Phase 5 - Introduce CMake In Parallel

### Recommendation

CMake is a reasonable modernization target, but it should be added in parallel first rather than replacing Autotools immediately.

### Why

- VS Code support is better and more familiar for many contributors.
- Windows workflows are usually easier to automate with CMake.
- Replacing the only known build system too early would increase risk.

### Practical Approach

1. Keep Autotools as the current reference build.
2. Add `CMakeLists.txt` and validate feature parity incrementally.
3. Compare build outputs and dependency handling.
4. Only switch defaults after the CMake path is proven stable.

## Phase 6 - Improve The Existing GTK Interface

### Recommendation

Improve the current UI before considering a toolkit rewrite.

### Why

- The application already has a working native UI stack.
- Many likely gains are in layout, clarity, workflow, and onboarding rather than toolkit choice.
- Running the app first will reveal which interface problems are real.

### Expected Areas Of Improvement

- typography and spacing
- control grouping and hierarchy
- visibility of calibration-related actions
- clearer first-run guidance in the interface
- better behavior on smaller screens

## Phase 7 - Re-Evaluate C++

### Current Recommendation

Do not start with a full migration from C to C++.

### Reasoning

- The current dependencies already work well from C.
- A language migration does not solve the immediate build and reliability problems.
- The cost of converting the codebase would be high relative to current needs.

### When C++ Could Make Sense

- if a new subsystem becomes significantly more complex
- if resource management and state modeling become hard to maintain in C
- if new modules can be introduced incrementally behind a C-compatible boundary

## Immediate Next Step

Start Phase 1.

Specifically:

1. inspect the Windows toolchain available on this machine
2. verify what is missing for MSYS2-based builds
3. make the app build from inside VS Code
4. run it locally

## Success Criteria For The First Milestone

The first milestone is complete when all of the following are true:

- the project can be configured and built on Windows from VS Code
- the executable can be launched locally
- the developer workflow is documented in-repo
- the next reliability fixes can be implemented against a working local build