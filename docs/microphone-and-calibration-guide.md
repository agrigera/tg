# Microphone And Calibration Guide

## Why This Guide Exists

Tg can work well, but it is not magic.

Most bad results come from one of these causes:

- weak or noisy microphone input
- poor watch-to-microphone positioning
- room noise
- wrong beat rate or lift angle settings
- missing or unstable calibration

If you want useful numbers, the audio setup matters almost as much as the software.

## What Kind Of Microphone Works Best

### Better options

- a dedicated external microphone
- a decent USB microphone
- an earbud or headset microphone placed directly against the watch case or holder

### Usually worse options

- laptop built-in microphones
- distant desktop microphones
- microphones with aggressive noise cancellation or automatic processing enabled

External community feedback repeatedly suggests that even simple earbud microphones can work better than expected when they are physically close to the watch.

## Placement Tips

- place the microphone as close as possible to the case, caseback, or movement holder
- keep the setup stable so the watch does not move during measurement
- reduce contact noise from fingers, table bumps, cables, and fans
- try several positions on the watch if the signal looks weak or inconsistent

In practice, small changes in position can make a large difference.

## Environment Tips

- use a quiet room
- turn off nearby fans, speakers, and loud electronics if possible
- avoid touching the desk during calibration or measurement
- if your microphone gain is adjustable, increase it only enough to capture the watch clearly without obvious clipping or distortion

## First-Run Checklist

Before trusting the numbers, verify this short list:

1. The operating system can see the microphone.
2. Tg is using a working audio input path.
3. The watch is close enough to the microphone to produce a stable signal.
4. The selected BPH is correct, or the guessed value is plausible.
5. The lift angle is correct if amplitude matters to you.
6. Calibration has been performed or the calibration value is known.

## Using Audio And Rate Controls

Recent builds add two real-time controls in the top bar:

- `audio`: choose the input device used by Tg
- `rate`: choose requested input sample rate

The `audio` list marks the default system input with `(default)`, and `rate` is displayed with readable kHz labels.

### Suggested Order

1. Select the microphone in `audio` first.
2. Keep `rate` at `44100` initially.
3. Verify that signal lock is stable.
4. Only then try `48000` or `96000` if needed.

### What Happens When You Change Them

- Tg restarts the audio input and compute pipeline automatically.
- If a selected sample rate is not supported by the device, Tg falls back to `44.1 kHz` (`44100`).
- Device and rate values are saved in config and reused on next start.

### Practical Advice

- If lock is unstable, try another microphone before increasing sample rate.
- Higher sample rates are not always better on noisy or low-quality microphones.
- Use the lowest rate that gives stable, repeatable lock for your setup.

## Why Calibration Exists

Many users expect software timing to already be exact because the computer has a clock.

That assumption is not good enough for this use case.

Tg is estimating watch performance from audio timing, and small errors in the effective sampling rate or timing reference can translate into meaningful error in seconds per day.

Calibration exists to compensate for the real behavior of the specific machine, audio path, and environment you are using.

## Practical Calibration Advice

Based on project docs and external user feedback:

- use a reliable and clearly audible reference source
- a loud 1 Hz quartz tick source is a practical choice
- keep the microphone very close to that source
- perform calibration in a quiet room
- repeat the calibration more than once if the result is unstable

If repeated runs differ materially, do not trust the first number you got. Fix the setup first.

## When Results Look Wrong

If Tg gives poor or inconsistent readings, check these in order:

### 1. Microphone quality and positioning

This is the most common source of trouble.

### 2. Wrong BPH or lift angle

If BPH is wrong, the rate interpretation can be wrong even if the signal is otherwise decent.

### 3. Calibration not performed or not repeatable

An unstable calibration often means the environment or reference source is not good enough.

### 4. Platform-specific audio problems

On macOS in particular, external reports point to microphone permission and PortAudio-related setup problems in some environments.

### 5. Built-in audio processing by the OS or hardware

Noise suppression, gain control, or other enhancements may interfere with measurement.

## Platform Notes

### macOS

- verify microphone permissions for the application or terminal environment used to launch it
- if audio input fails entirely, check operating system permission settings before assuming the application is broken

### Windows

- verify the selected/default input device is actually receiving signal
- if building from source, expect extra environment setup compared with Linux

### Linux

- confirm the microphone path is working and not heavily processed by the desktop audio stack
- if measurements differ unexpectedly across systems, compare audio settings before blaming the algorithm

## What To Expect From Tg

Tg is best understood as a capable software timegrapher for technically minded users and hobbyists.

If the setup is good, it can provide useful, repeatable measurements.

If the setup is poor, it can mislead you very quickly.

That is why microphone guidance and calibration discipline matter.

## Related Reading

- [docs/project-overview.md](project-overview.md)
- [docs/tg-timer.1](tg-timer.1)
- [docs/issues/external-feedback-2026-03-22.md](issues/external-feedback-2026-03-22.md)
- [docs/issues/backport-progress-2026-03-22.md](issues/backport-progress-2026-03-22.md)
