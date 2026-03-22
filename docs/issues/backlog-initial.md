# Initial Backlog

This backlog converts the 2026-03-22 review into issue-ready items.

## Ready To Open

### [BUG] Empty snapshot files fail to load

- Priority: P1
- Labels: `bug`
- Files: `src/serializer.c`
- Acceptance criteria:
- `write_file()` output with zero snapshots can be loaded successfully.
- The reader cleanly distinguishes between valid empty input and malformed input.

### [DEBT] Replace Windows double bit-cast with a safe implementation

- Priority: P2
- Labels: `technical-debt`
- Files: `src/serializer.c`
- Acceptance criteria:
- No pointer-cast aliasing is used for double-to-bits conversion.
- Windows serialization remains backward compatible with existing files.

### [DEBT] Add cleanup for `start_computer()` initialization failures

- Priority: P2
- Labels: `technical-debt`
- Files: `src/computer.c`
- Acceptance criteria:
- All allocations made before thread start are released on failure.
- Mutex and condition variable initialization failures are handled safely.

### [DOCS] Update repository URLs and stale build references

- Priority: P2
- Labels: `documentation`
- Files: `README.md`, `configure.ac`
- Acceptance criteria:
- Repository URLs point to the current remote.
- The top-level status badge and clone commands are current or removed.

### [DOCS] Align README with existing manual and current distribution status

- Priority: P3
- Labels: `documentation`
- Files: `README.md`, `docs/tg-timer.1`
- Acceptance criteria:
- README no longer claims that no manual exists.
- Download and packaging guidance is either verified or clearly marked as historical.

### [DOCS] Fix typo in man page synopsis

- Priority: P3
- Labels: `documentation`
- Files: `docs/tg-timer.1`
- Acceptance criteria:
- The typo is corrected with no other content regressions.
