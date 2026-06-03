# model_only

Raw model with no memory/retrieval layer. For memory benchmarks, uses the full
conversation transcript as context (no retrieval). For coding/text tasks,
prompts the model directly with the task.

## Capabilities

- `memory_answer`, answers questions using full transcript context
- `text_answer`, direct question answering
- `code_complete`, code completion tasks

## Notes

- `retrieved_tokens` is always 0 (no retrieval step)
- `assembled_context_tokens` reflects total context tokens fed to the model
- Non-deterministic due to LLM sampling
