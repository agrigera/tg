# [DOCS] Align README with current project state

## Summary

The README is out of sync with the current repository contents and project status.

## Priority

- P3 Low

## Labels

- `documentation`

## Current Behavior

- README says the project still has no manual.
- A man page already exists in `docs/tg-timer.1`.
- README references external binary download locations that were not validated in the current review.
- Some external links use outdated destinations or protocols.

## Evidence

- Files: `README.md`, `docs/tg-timer.1`
- Review date: 2026-03-22

## Expected Documentation

The README should describe the current project artifacts accurately, including the presence of the manual and the status of build/distribution guidance.

## Proposed Fix

Revise the README to reflect current documentation availability, verify or clearly qualify distribution links, and modernize stale references.

## Acceptance Criteria

- [ ] README no longer claims that there is no manual.
- [ ] The manual is referenced consistently from the README.
- [ ] Distribution/build references are either verified or marked as historical/unverified.
