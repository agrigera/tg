# [BUG] Empty snapshot files fail to load

## Summary

`write_file()` can generate a valid `.tgj` file with zero snapshots, but `read_file()` rejects that same file on input.

## Priority

- P1 High

## Labels

- `bug`
- `serialization`

## Impact

This breaks round-trip behavior for a valid file format case. A file that represents an empty snapshot list cannot be loaded back successfully.

## Evidence

- File: `src/serializer.c`
- Writer path allows `cnt == 0` in `write_file()`.
- Reader path in `read_file()` reaches end-of-structure successfully but only returns success if `*s` is non-null.
- The logic near end-of-file handling treats a valid empty parse as an error path.

## Steps To Reproduce

1. Create or save a `.tgj` file whose snapshot list length is zero.
2. Call the load path that uses `read_file()`.
3. Observe that the file is rejected instead of being loaded as an empty result.

## Expected Result

An empty but valid snapshot list should be accepted and loaded as zero snapshots.

## Actual Result

The reader reports failure and drops into cleanup/error handling.

## Root Cause Hypothesis

The success condition in `read_file()` is coupled to the presence of at least one parsed snapshot instead of the validity of the file structure.

## Acceptance Criteria

- [ ] `write_file()` output with zero snapshots is accepted by `read_file()`.
- [ ] Valid empty input is distinguishable from malformed input.
- [ ] Existing non-empty `.tgj` files still load correctly.

## Notes

This is the highest-priority confirmed bug found in the 2026-03-22 review.
