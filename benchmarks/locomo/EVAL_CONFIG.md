# LoCoMo Harness v2

- Direct track uses `aimee memory search` against an isolated temporary home served by `aimee-server`/`aimee-kb` and populated from a single LoCoMo conversation.
- LLM track uses `aimee delegate execute`, so answer generation and judging flow through the execute-role agent configured in `~/.config/aimee/agents.json`.
- Judge policy is a 3-run majority vote with the shared JSON scorer prompt.
- Cost estimation is best-effort and only emitted when the execute-role model matches a pricing entry in Aimee's token tracker.
