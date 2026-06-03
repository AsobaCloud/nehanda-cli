# Working-Profile Operational Validation Status

Status date: 2026-05-27

## Current Evidence

- `2026-05-19.json` exists as the prior snapshot.
- `2026-05-27.json` was captured on 2026-05-27.
- `2026-05-27.diff.txt` compares 2026-05-19 to 2026-05-27 with
  `--flip-threshold 0.3`.
- The 2026-05-27 diff reports no added, removed, changed, or
  high-confidence flip candidates.

Both committed snapshots currently have `working_profile.entry_count`
equal to `0`. That means this validation is started, but the month-long
flag-on validation cannot truthfully begin yet: there is no
highest-confidence working-profile field to enable.

## Missing Before Move To Done

- Capture at least three weekly snapshot/diff pairs with real
  working-profile state or explicit evidence that no profile state was
  learned during the observation window.
- Once a working-profile field has committed confidence, enable only
  that field and record the exact enablement date.
- Add `first-field-writeup.md` documenting the pre/post behaviour
  comparison for the enabled field.
- After one real month with the field enabled, add a closeout judgement
  covering usefulness, bounded drift, and any retroactive-review
  outcomes.

## Revised Operational Checkpoints

Because the original scheduled artifacts were not present in the repo,
the evidence cycle restarts from the first captured current snapshot.

| Date | Action |
|---|---|
| 2026-05-27 | Current snapshot and diff captured. Working profile is empty. |
| 2026-06-03 | Capture next weekly snapshot and diff against 2026-05-27. |
| 2026-06-10 | Capture next weekly snapshot and diff against 2026-06-03. |
| 2026-06-17 | Capture next weekly snapshot and diff against 2026-06-10. |
| First non-empty high-confidence field | Enable only that field and write `first-field-writeup.md`. |
| One month after enablement | Write the month-end validation closeout and mark this validation complete if all criteria pass. |

## Daily Automation

Two local Aimee cron jobs were created on 2026-05-27:

| Job | Schedule | Action |
|---|---|---|
| `working-profile-daily-snapshot` | `0 7 * * *` | Runs `./scripts/working-profile-operational-cycle.sh snapshot` to capture a snapshot and diff against the prior snapshot. |
| `working-profile-daily-status` | `0 8 * * *` | Runs `./scripts/working-profile-operational-cycle.sh status` to record readiness and the next required action in cron history. |

The cron jobs currently run in the session worktree used for this PR.
After merge, they should be re-pointed to the main checkout if this
session worktree is removed.
