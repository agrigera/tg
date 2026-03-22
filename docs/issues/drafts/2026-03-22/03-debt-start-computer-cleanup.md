# [DEBT] Add cleanup for start_computer() initialization failures

## Summary

`start_computer()` allocates multiple resources before initializing synchronization primitives and the worker thread, but returns `NULL` without cleanup when one of those initialization steps fails.

## Priority

- P2 Medium

## Labels

- `technical-debt`
- `memory`
- `threading`

## Why It Matters

Error-path leaks make initialization failures harder to debug and reduce confidence in the code's failure handling. They also complicate future refactors.

## Evidence

- File: `src/computer.c`
- Function: `start_computer()`
- If `pthread_mutex_init`, `pthread_cond_init`, or `pthread_create` fails, the function reports an error and returns `NULL`.
- At that point, previously allocated buffers, state objects, and snapshots are still live.

## Risk

- Medium

## Proposed Direction

Add a single cleanup path that releases all allocations performed before thread start, and destroy any partially initialized synchronization primitives safely.

## Acceptance Criteria

- [ ] All allocations made before thread creation are released on failure.
- [ ] Partially initialized mutex and condition resources are handled correctly.
- [ ] The normal success path is unchanged.

## Notes

This is a reliability and maintainability issue rather than a user-visible functional bug.
