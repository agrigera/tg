# Local Issue Register - 2026-03-22

This register consolidates local findings and imported issue candidates from upstream forks/repositories.

Status values:

- `resolved-local`: implemented in this repository during the 2026-03-22 session
- `open-local`: identified and still pending implementation

## Consolidated List

| ID | GitHub | Title | Source | Priority | Local Status | GitHub State |
|---|---|---|---|---|---|---|
| L-001 | #4 | Empty snapshot files fail to load | Local audit | P1 | resolved-local | closed |
| L-002 | #5 | Replace Windows double bit-cast with safe serialization | Local audit | P2 | resolved-local | closed |
| L-003 | #6 | Add cleanup for `start_computer()` initialization failures | Local audit | P2 | resolved-local | closed |
| L-004 | #7 | Update repository URLs and stale build references | Local audit | P2 | resolved-local | closed |
| L-005 | #8 | Align README with current project state | Local audit | P3 | resolved-local | closed |
| L-006 | #9 | Fix typo in man page synopsis | Local audit | P3 | open-local | open |
| L-007 | #1 | Add microphone setup guidance for reliable measurements | Local audit | P2 | resolved-local | closed |
| L-008 | #2 | Add macOS audio-input troubleshooting guidance | Local audit | P2 | resolved-local | closed |
| L-009 | #3 | Explain calibration and first-run setup | Local audit | P2 | resolved-local | closed |
| L-010 | #10 | Investigate CENTER button crash on macOS | vacaboja/tg #41 triage | P1 | open-local | open |
| L-011 | #11 | Allow manual audio input device selection | vacaboja/tg #38 triage | P1 | resolved-local | closed |
| L-012 | #12 | Improve low-BPH detection and clock support | vacaboja/tg #36/#11 triage | P1 | resolved-local | closed |
| L-013 | #13 | Evaluate standard export format for snapshots | vacaboja/tg #33 triage | P2 | open-local | open |
| L-014 | #14 | Clean up signed/unsigned comparison in event location code | vacaboja/tg #16 triage | P3 | resolved-local | closed |
| X-012 | #15 | Allow importing wav/flac/m4a to initialize snapshots | xyzzy42/tg #12 | P2 | open-local | open |
| X-013 | #16 | UI labels/tooltips/verdicts for measured fields | xyzzy42/tg #13 + PR #14 | P3 | open-local | open |
| X-008 | #17 | Improve Windows compile instructions and troubleshooting | xyzzy42/tg #8 | P2 | resolved-local | closed |
| X-005 | #18 | macOS microphone/input troubleshooting and diagnostics | xyzzy42/tg #5 | P2 | open-local | open |
| X-002 | #19 | Fails to start while JackD is running | xyzzy42/tg #2 | P2 | open-local | open |
| X-006 | #20 | Support for cylinder/pin lever movements | xyzzy42/tg #6 | P3 | open-local | open |
| X-007 | #21 | Very high BPH support (e.g. 360000) | xyzzy42/tg #7 | P3 | open-local | open |

## Publication Plan

All items above have been mirrored as GitHub issues in this repository for visibility.

- `resolved-local` items were created and then closed with traceability comments.
- `open-local` items were created and left open for planning and execution.

## Short Roadmap (Top 5 Open)

Recommended execution order based on user impact and implementation risk.

1. **#10** — Investigate CENTER button crash on macOS (`L-010`, P1)
	- Rationale: potential crash path, highest reliability impact.

2. **#13** — Evaluate standard export format for snapshots (`L-013`, P2)
	- Rationale: improves long-term data interoperability and maintenance.

3. **#15** — Allow importing wav/flac/m4a to initialize snapshots (`X-012`, P2)
	- Rationale: high user value for offline analysis and historical comparisons.

4. **#18** — Investigate macOS microphone/input signal detection failures (`X-005`, P2)
	- Rationale: recurring platform usability issue affecting first-run success.

5. **#19** — Fails to start while JackD is running (`X-002`, P2)
	- Rationale: Linux audio backend compatibility issue with clear operational impact.

Deferred after top 5:

- **#9** typo in man page (low effort, low impact)
- **#16** labels/tooltips panel UX (nice-to-have, not reliability-critical)
- **#20** cylinder/pin lever support (bigger algorithm scope)
- **#21** very high BPH support (specialized use case)