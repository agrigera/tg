# [DEBT] Replace Windows double bit-cast with a safe implementation

## Summary

The Windows-specific serializer converts a `double` to `uint64_t` through pointer casting, which relies on undefined behavior under strict aliasing rules.

## Priority

- P2 Medium

## Labels

- `technical-debt`
- `windows`
- `serialization`

## Why It Matters

This code may work on current toolchains by accident, but it is not portable or optimizer-safe. It increases maintenance risk on Windows and makes the serializer harder to trust.

## Evidence

- File: `src/serializer.c`
- Current code: `uint64_t bd = *(uint64_t *)&d;`

## Risk

- Medium

## Proposed Direction

Replace the pointer cast with a bit-preserving `memcpy` between `double` and `uint64_t`, preserving the existing on-disk format.

## Acceptance Criteria

- [ ] No strict-aliasing-unsafe pointer cast is used for `double` bit extraction.
- [ ] Existing Windows serialization output remains backward compatible.
- [ ] The corresponding parse/write code remains consistent and documented.

## Notes

This is not a confirmed end-user bug today, but it is a concrete portability defect.
