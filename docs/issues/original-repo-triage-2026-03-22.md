# Original Repository Issue Triage - 2026-03-22

Source repository reviewed: [vacaboja/tg issues](https://github.com/vacaboja/tg/issues)

## Purpose

This document reviews the issues from the original upstream repository and decides which ones still make sense for the current repository.

The goal is not to copy everything blindly. The goal is to keep only items that are still relevant to the current codebase, current product scope, or current documentation gaps.

## High-Level Result

- Open issues upstream: 17
- Closed issues upstream: 4
- Good candidates to import now: 8
- Better handled as future roadmap items: 4
- Not worth importing as-is: 5

## Import Now

### Import: #41 Crash when clicking CENTER on the trace on MacOS Ventura

- Upstream: [#41](https://github.com/vacaboja/tg/issues/41)
- Type: bug
- Decision: import
- Why:
- The upstream crash report points directly at `handle_center_trace`.
- The current codebase still contains that handler in `src/output_panel.c`.
- Upstream discussion suggests the bug was fixed in another fork, not in this repository.
- Recommended local title:
- `[BUG] Investigate CENTER button crash on macOS`

### Import: #38 Manually select audio device

- Upstream: [#38](https://github.com/vacaboja/tg/issues/38)
- Type: feature
- Decision: import
- Why:
- Current code still opens only the default input device through PortAudio.
- The request is concrete and broadly useful.
- Upstream comments indicate the feature exists in another fork, which makes it a realistic candidate rather than a speculative request.
- Recommended local title:
- `[FEATURE] Allow manual audio input device selection`

### Import as one merged item: #36 TG struggles or fails to detect anything below 18000 BPH + #11 Support for low BPH clocks

- Upstream: [#36](https://github.com/vacaboja/tg/issues/36), [#11](https://github.com/vacaboja/tg/issues/11)
- Type: bug or feature gap
- Decision: import as a single merged issue
- Why:
- These are the same underlying problem family.
- #36 reports failure below 18000 BPH.
- #11 asks for support for very low BPH clocks.
- Upstream comments say a fix exists in another fork but was not merged here.
- Recommended local title:
- `[BUG] Improve low-BPH detection and clock support`

### Import: #33 Use standard format for snapshot data

- Upstream: [#33](https://github.com/vacaboja/tg/issues/33)
- Type: feature or technical debt
- Decision: import
- Why:
- The current repository still uses a custom snapshot format.
- We already found a confirmed serializer bug locally, so the format area is active technical debt.
- Even if a format migration is not done immediately, the issue is valid and well-scoped.
- Recommended local title:
- `[DEBT] Evaluate standard export format for snapshots`

### Import in documentation form: #22 Error opening audio input Mac OS Big Sur 11.1

- Upstream: [#22](https://github.com/vacaboja/tg/issues/22)
- Type: troubleshooting or platform-specific runtime issue
- Decision: import, but as docs and troubleshooting rather than as a core code bug
- Why:
- The upstream discussion points to microphone permissions and PortAudio packaging, not a confirmed application logic bug.
- The behavior is still useful to document for macOS users.
- Recommended local title:
- `[DOCS] Add macOS audio-input troubleshooting and microphone permission guidance`

### Import: #20 Microphone guidance

- Upstream: [#20](https://github.com/vacaboja/tg/issues/20)
- Type: documentation
- Decision: import
- Why:
- The request is valid.
- We already added a high-level overview doc, but not practical setup guidance.
- This still fills a real documentation gap.
- Recommended local title:
- `[DOCS] Add microphone setup guidance for reliable measurements`

### Import: #14 Problems building for windows

- Upstream: [#14](https://github.com/vacaboja/tg/issues/14)
- Type: build documentation or packaging issue
- Decision: import
- Why:
- Windows build instructions are clearly fragile and partially historical.
- The upstream issue includes extra environment details that are missing from the current README.
- This fits the current repository because Windows is still documented as supported.
- Recommended local title:
- `[DOCS] Update and validate Windows build instructions`

### Import: #16 Integer comparison

- Upstream: [#16](https://github.com/vacaboja/tg/issues/16)
- Type: technical debt
- Decision: import, low priority
- Why:
- The referenced code pattern is still present in `src/algo.c`.
- This is not a user-facing bug, but it is a legitimate warning cleanup item.
- Recommended local title:
- `[DEBT] Clean up signed/unsigned comparison in event location code`

## Future Roadmap, But Not Immediate Imports

### Defer: #24 Continuous auto-calibration using audio timestamps

- Upstream: [#24](https://github.com/vacaboja/tg/issues/24)
- Decision: defer
- Why:
- The idea is technically meaningful.
- It is also non-trivial and design-heavy.
- This belongs in a roadmap discussion rather than the immediate backlog.

### Defer: #40 implementing self-calibration

- Upstream: [#40](https://github.com/vacaboja/tg/issues/40)
- Decision: defer
- Why:
- This overlaps strongly with #24.
- It introduces a concrete implementation idea, but it needs design review and probably platform-specific validation.
- Best treated together with #24 as a future calibration-improvement track.

### Defer: #25 Long term time strip graph

- Upstream: [#25](https://github.com/vacaboja/tg/issues/25)
- Decision: defer
- Why:
- This is a real product enhancement.
- It is not as urgent as the confirmed bugs, build issues, or documentation gaps.

### Defer: #26 Custom periodic tick marks on the time strip graph

- Upstream: [#26](https://github.com/vacaboja/tg/issues/26)
- Decision: defer
- Why:
- Interesting feature for deeper diagnostics.
- Specialized and lower priority than correctness and maintenance work.

## Do Not Import As-Is

### Do not import: #43 TG version for Linux?

- Upstream: [#43](https://github.com/vacaboja/tg/issues/43)
- Decision: do not import as-is
- Why:
- The original reporter later indicated the issue was probably caused by sound-card settings.
- There is no strong evidence of an application defect in the final state of the discussion.

### Do not import: #39 Option for tuning fork watches

- Upstream: [#39](https://github.com/vacaboja/tg/issues/39)
- Decision: do not import for now
- Why:
- This is a scope expansion beyond the current stated focus on mechanical watches.
- It may be interesting later, but it should not enter the main backlog without an explicit product decision.

### Do not import: #37 Idea - timing

- Upstream: [#37](https://github.com/vacaboja/tg/issues/37)
- Decision: do not import as a separate issue
- Why:
- It is essentially a variant of the continuous calibration idea in #24.
- Importing it separately would only duplicate backlog noise.

### Do not import now: #18 screen size problem for 7 inch lcd

- Upstream: [#18](https://github.com/vacaboja/tg/issues/18)
- Decision: do not import now
- Why:
- The request is legitimate, but it is highly device-specific.
- It also overlaps with broader layout and responsiveness work rather than a narrowly defined defect.

### Do not import: closed issues #8, #7, #6, #4

- Upstream: [#8](https://github.com/vacaboja/tg/issues/8), [#7](https://github.com/vacaboja/tg/issues/7), [#6](https://github.com/vacaboja/tg/issues/6), [#4](https://github.com/vacaboja/tg/issues/4)
- Decision: do not import
- Why:
- These are already closed upstream and there is no evidence from the current review that they need reopening locally.

## Recommended Next Step

If we want to formalize this triage in the current repository, the best next move is to open or draft local issues for these imported candidates:

1. `[BUG] Investigate CENTER button crash on macOS`
2. `[FEATURE] Allow manual audio input device selection`
3. `[BUG] Improve low-BPH detection and clock support`
4. `[DEBT] Evaluate standard export format for snapshots`
5. `[DOCS] Add macOS audio-input troubleshooting and microphone permission guidance`
6. `[DOCS] Add microphone setup guidance for reliable measurements`
7. `[DOCS] Update and validate Windows build instructions`
8. `[DEBT] Clean up signed/unsigned comparison in event location code`
