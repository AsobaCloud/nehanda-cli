# LongMemEval Harness v2

- Direct track uses `aimee memory search` against an isolated temporary home served by `aimee-server`/`aimee-kb` and populated from the case haystack sessions.
- LLM track uses `aimee delegate execute`, so answer generation and judging stay inside Aimee's normal execute-role routing.
- Subset labels are taken from dataset metadata when present and fall back to conservative heuristics when absent.
- Cost estimation is best-effort and only emitted when the execute-role model matches a pricing entry in Aimee's token tracker.
