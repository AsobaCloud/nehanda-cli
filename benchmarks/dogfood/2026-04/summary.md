# Dogfood Review: 2026-04

## Summary

- Records reviewed by report source: 38,522
- Labelled records: 2,625
- Unlabelled records remaining: 35,897
- Auto-labelled records: 0
- Retrieved-zero records: 0

## Finding

The first April dogfood cycle exposed operational blockers before it exposed a
model-quality finding: the month-scale `dogfood.report` response was too large
for the client/server RPC framing path, so the official report command failed
before the JSON could be delivered to the CLI. The review reminder path did
complete one armed reminder, but the report path could re-arm a completed
monthly reminder on a later run.

## Action

This PR fixes those aimee reliability issues:

- `aimee jobs show <id>` now routes to the same delegate job status RPC as
  `aimee jobs status <id>`, so delegate supervision does not dead-end on a
  common status/show alias.
- Non-streaming CLI RPC reads now grow to a bounded 16 MiB response buffer.
- Oversized synchronous server responses use a bounded direct write path
  instead of failing when they do not fit in the 256 KiB connection queue.
- Dogfood review reminders are only armed once per month, even after the
  previous reminder has been completed.

The committed `report.json` was generated from the April JSONL source after the
installed server failed to return the oversized `dogfood.report` response. The
code changes above are the derived reliability PR from this cycle.

## Artefacts

- `report.json`: April aggregate report generated from
  `/home/virant/.config/aimee/dogfood/2026-04.jsonl`.
- `review.json`: observed `dogfood review --month 2026-04 --limit 0 --json`
  result, including the completed review reminder.
- `reminder-demo.before.json` / `reminder-demo.after.json`: compact evidence
  for the armed reminder, prospective surfacing, and completion transition.
