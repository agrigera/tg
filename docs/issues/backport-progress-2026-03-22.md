# Backport Progress - 2026-03-22

## Scope

This note tracks implementation work completed in this repository during the 2026-03-22 maintenance and backport session.

It focuses on concrete code changes that were actually implemented and build-validated.

## Release Notes Draft

### Highlights

- Added paperstrip zoom controls and improved centering/shift behavior.
- Added tic/toc-aware event rendering and amplitude history overlay.
- Added manual audio input device and sample-rate controls in the main UI.
- Added safe audio restart behavior with automatic fallback to 44.1 kHz when needed.
- Improved low-BPH handling by replacing a fixed period cutoff with a BPH-aware limit.
- Extended snapshot persistence for new trace and history fields.

### Breaking Changes

- No intentional user-facing breaking change in file format or workflow.
- Existing snapshots remain supported through backward-compatible defaults in the serializer.

### Migration Notes

- Config now stores `audio_device` and `audio_rate`.
- On first launch after update, users should verify selected `audio` and `rate` controls.
- If a selected rate is unsupported by the current input device, the app falls back to 44.1 kHz and continues running.

### GitHub Release Copy (Short)

Tg maintenance release with usability, reliability, and persistence improvements.

#### Highlights

- Added paperstrip zoom controls with improved centering and shift behavior.
- Added tic/toc-aware event rendering and amplitude history overlay.
- Added audio input device and sample-rate controls in the top bar.
- Added automatic fallback to 44.1 kHz when a selected sample rate is unsupported.
- Improved low-BPH handling by replacing a fixed period cutoff with a BPH-aware limit.
- Extended snapshot persistence for new trace/event/amplitude fields with backward-compatible defaults.

#### Notes

- Existing snapshot files remain supported.
- Config now includes `audio_device` and `audio_rate`.
- After updating, verify selected `audio` and `rate` once on first run.

## Completed Batches

### 1. Snapshot and event model expansion

Implemented:

- event type storage (`tic` vs `toc`) in snapshot and processing buffers
- amplitude history ring in snapshots (values + timestamps)
- trace zoom state in snapshots

Files:

- `src/tg.h`
- `src/algo.c`
- `src/computer.c`

## 2. Persistence and compatibility updates

Implemented:

- serializer support for `events_tictoc`
- serializer support for amplitude history arrays and write pointer
- serializer support for `trace_zoom`
- backward-compatible defaults when new fields are missing
- Windows-safe `double` serialization bit handling via `memcpy`
- read-path fix to support valid files with empty snapshot lists

Files:

- `src/serializer.c`

## 3. Paperstrip visual and interaction upgrades

Implemented:

- event color split by tic/toc
- amplitude trend overlay in paperstrip
- improved center and shift behavior
- real zoom behavior in paperstrip math
- zoom controls (`-`, `1x`, `+`)
- clear-trace now resets zoom to baseline

Files:

- `src/output_panel.c`
- `src/interface.c` (state carry-over across snapshot refresh)

## 4. Audio input selection (device + sample rate)

Implemented:

- manual audio input device selector in main UI
- sample-rate selector in main UI
- restart path that reinitializes PortAudio and computer pipeline after selection changes
- fallback to 44.1 kHz if the selected sample rate fails on the chosen device
- config persistence for selected audio device and audio rate

Files:

- `src/audio.c`
- `src/interface.c`
- `src/tg.h`
- `src/config.c` (through existing `CONFIG_FIELDS` wiring)

## 5. Initialization and cleanup reliability work

Implemented:

- improved failure-path cleanup in `start_computer()` initialization

Files:

- `src/computer.c`

## 6. Low-BPH detection support

Implemented:

- replaced fixed period cutoff in processing (`period >= sample_rate/2`) with BPH-aware max period guard
- lowered minimum accepted BPH to include lower-frequency clocks

Files:

- `src/algo.c`
- `src/tg.h`

## 7. Audio controls UX polish

Implemented:

- audio input combo now marks the default device in the visible label
- sample-rate combo now shows human-readable labels in kHz

Files:

- `src/interface.c`

## Build Validation

Build and smoke tests were repeatedly run in the documented Windows/MSYS2 workflow:

- `C:\msys64\msys2_shell.cmd -defterm -no-start -mingw64 -here -c "make -j4"`
- `C:\msys64\msys2_shell.cmd -defterm -no-start -mingw64 -here -c "./tg-timer.exe"`

Observed result in this session:

- compile and link successful after each finalized batch
- no immediate startup/runtime errors in short smoke runs

## Current Status

The repository now includes:

- richer event/amplitude trace data flow
- persistence of the expanded snapshot state
- functional paperstrip zoom controls
- configurable audio device and sample rate in UI
- improved handling of lower-BPH timing cases

User-facing guidance for the new controls is documented in:

- `docs/microphone-and-calibration-guide.md`

## Follow-up Candidates

Potential next backport targets discussed but not yet implemented in this note:

- deeper audio-device UX polish
- additional troubleshooting and user-facing docs for new UI controls