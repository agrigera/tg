# [DOCS] Update repository URLs and stale build references

## Summary

The repository currently tracks `https://github.com/agrigera/tg`, but project metadata and documentation still reference `vacaboja/tg` and an old Travis CI badge.

## Priority

- P2 Medium

## Labels

- `documentation`
- `metadata`

## Current Behavior

The top-level README and `configure.ac` still point to the former repository location, which makes clone instructions and embedded project URLs stale.

## Evidence

- Files: `README.md`, `configure.ac`
- Examples found during review:
- `README.md` badge references `travis-ci.org/vacaboja/tg`
- `README.md` clone commands use `https://github.com/vacaboja/tg.git`
- `configure.ac` embeds `https://github.com/vacaboja/tg`

## Expected Documentation

Repository references should match the active remote or be intentionally removed if no longer maintained.

## Proposed Fix

Update or remove stale URLs, replace the old status badge, and align clone instructions with the current repository location.

## Acceptance Criteria

- [ ] Public repository URLs match the current canonical remote.
- [ ] The README no longer points to obsolete CI infrastructure.
- [ ] Build and clone instructions remain internally consistent.
