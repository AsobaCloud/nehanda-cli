# small_agent

Aimee + small local GGUF model, CPU-only. Uses AimeeHarness with a small local
model (Qwen3 3B-class GGUF via llama.cpp).

## Model Configuration

- Default: `~/.local/share/aimee/models/small_agent.gguf`
- Override via `AIMEE_SMALL_AGENT_MODEL` environment variable

## Capabilities

- `memory_ingest`, stores facts in aimee memory
- `memory_answer`, retrieves and answers using memory
- `code_complete`, code completion tasks

## Notes

- Falls back to configured execute-role model if GGUF not found
- Non-deterministic due to LLM sampling
