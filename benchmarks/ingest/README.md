# Ingest Lab Fixture Corpus

Fixture corpus for the ingest lab and strategy-aware chunking work.

## Shapes

Three fixture files follow the charter replay harness format:

| File | Purpose |
|------|---------|
| `fixtures/positive.jsonl` | Well-formed documents that the lab should classify as `ready` |
| `fixtures/false-positive.jsonl` | Documents that look well-formed but should trigger `review_needed` signals |
| `fixtures/regression.jsonl` | Known edge cases (empty files, extension detection, heading tracking) that must not regress |

## Fixture Schema

Each JSONL line:

```json
{
  "id": "pos-001",
  "description": "human-readable description",
  "doc_kind": "markdown|code|text|unknown",
  "extension": ".md",
  "content": "document text",
  "expected_stage": "ready|review_needed|reject",
  "expected_signals": ["flat_text", "heading_skip", ...],
  "expected_chunk_count_min": 2
}
```

Signal names map to `kb_lab_flag_t` values in `src/headers/kb_lab.h`.

## Running the Evaluator

```bash
python3 benchmarks/ingest/eval.py
```

The evaluator auto-detects the `aimee` binary at `build/aimee`. If not found,
it exits 0 with a warning (so CI does not fail when the binary is absent).

```bash
python3 benchmarks/ingest/eval.py --aimee /path/to/aimee --verbose
```

## Adding Fixtures

- Add positive fixtures for each document kind that should produce clean output.
- Add false-positive fixtures for each quality signal the lab can emit.
- Add regression fixtures for any bug or edge case that was fixed.
- Keep content strings short and focused on the specific signal under test.
