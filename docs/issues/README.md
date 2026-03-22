# Issue Tracking Notes

This directory stores repository-tracked issue drafts and audit notes.

## Purpose

- Keep findings versioned before they become GitHub issues.
- Preserve technical context discovered during reviews.
- Make backlog grooming easier by separating confirmed findings from informal chat.

## Recommended Workflow

1. Record findings in a dated audit note.
2. Promote each actionable item to a GitHub issue using the templates in `.github/ISSUE_TEMPLATE/`.
3. Link the resulting issue number back into the audit note or backlog.
4. Link the fixing pull request to the issue.

## Priority Levels

- P0 Critical: data loss, security issue, or application unusable.
- P1 High: confirmed bug with real functional impact.
- P2 Medium: correctness, portability, or maintenance risk.
- P3 Low: typo, cleanup, or minor documentation problem.

## Issue Types

- `bug`: confirmed defect or regression.
- `documentation`: stale, wrong, or missing docs.
- `technical-debt`: cleanup or reliability work with no immediate user-visible bug.
- `audit`: temporary bucket for findings during a review.

## Current Audit Files

- `2026-03-22-project-status.md`
- `backport-progress-2026-03-22.md`
- `local-issue-register-2026-03-22.md`
- `backlog-initial.md`
- `action-plan-2026-03-22.md`
- `original-repo-triage-2026-03-22.md`
- `external-feedback-2026-03-22.md`
