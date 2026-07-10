# Self-Hosting

Run nehanda-cli entirely locally without the Nehanda Gateway or any cloud dependency. You supply your own model endpoint.

## When to use this

- You want to run a local model (Ollama, llama.cpp, vLLM on your own hardware)
- You want to use nehanda-cli's memory and delegation layer with a different fine-tune
- You don't have a Nehanda subscription yet

## Configure

In `config/nehanda.yaml` (copy from `config/nehanda.yaml.example`):

```yaml
self_host:
  endpoint: "http://localhost:11434/v1"   # any OpenAI-compatible endpoint
  model: "your-model-name"
```

Or via environment variable:
```bash
export NEHANDA_SELF_HOST_ENDPOINT=http://localhost:11434/v1
export NEHANDA_SELF_HOST_MODEL=llama3:latest
```

When `self_host.endpoint` is set:
- The Nehanda Gateway is bypassed entirely
- `NEHANDA_API_KEY` and auth are not required
- All other aimee features (memory, delegates, guardrails) work normally

## Example: use with your local Ollama

```bash
export NEHANDA_SELF_HOST_ENDPOINT=http://localhost:11434/v1
export NEHANDA_SELF_HOST_MODEL=deepseek-r1:14b
nehanda
```

## Example: use with the Windows LAN Ollama

```bash
export NEHANDA_SELF_HOST_ENDPOINT=http://AsobaCorp-1.local:11434/v1
export NEHANDA_SELF_HOST_MODEL=deepseek-coder-v2:latest
nehanda
```

## Limitations vs hosted Nehanda

| Feature | Self-hosted | Hosted Nehanda |
|---|---|---|
| Memory & compaction | ✓ | ✓ |
| Local delegate fan-out | ✓ | ✓ |
| Guardrails | ✓ | ✓ |
| Model quality | Depends on your model | Nehanda 27B fine-tune |
| Subscription required | No | Yes |
| Token costs | Your hardware / endpoint | Metered per tier |
