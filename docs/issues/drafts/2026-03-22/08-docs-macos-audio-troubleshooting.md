# [DOCS] Add macOS audio-input troubleshooting and permission guidance

## Summary

External reports suggest that some macOS users hit audio-input failures related to microphone permissions or PortAudio environment issues, but the project docs do not currently guide users through that path.

## Priority

- P2 Medium

## Labels

- `documentation`
- `macos`
- `audio`

## Why It Matters

When audio input fails, users often interpret the problem as a broken application rather than a platform setup issue. That creates avoidable support burden and abandonment.

## Evidence

- Upstream issue discussions reference macOS microphone permission problems.
- External forum search results show recurring macOS audio and microphone trouble.

## Proposed Fix

Document a short troubleshooting section for macOS covering:

- microphone permission checks
- verifying input availability at OS level
- confirming the launch context has permission to access the microphone
- when to suspect packaging or PortAudio-specific issues

## Acceptance Criteria

- [ ] macOS users have a documented troubleshooting path for audio input failures.
- [ ] The guide clearly distinguishes application bugs from permission/setup problems.
- [ ] The guidance is linked from the main docs.
