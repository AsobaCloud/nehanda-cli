# nehanda-cli

A local AI coding substrate powered by the **Nehanda** fine-tuned model. Free and open-source (AGPL-3.0).

Built on [aimee](https://github.com/RakuenSoftware/aimee) — a local memory substrate with 4-tier context compaction, supervisor/delegate task decomposition, and guardrails. This fork adds Nehanda as the primary reasoning model, routed through the Nehanda Gateway, with Zorora's auth infrastructure handling access control.

```
  Your Terminal (nehanda TUI, or any OpenAI-compatible CLI)
          │
          ▼
  nehanda-cli (local, AGPL)
  • 4-tier memory compaction (~86% token reduction)
  • Supervisor/Delegate task fan-out
  • Local Ollama delegates (free, zero egress)
  • Injects NEHANDA_API_KEY on upstream calls
          │  HTTPS — OpenAI/Anthropic wire protocol
          ▼
  Nehanda Gateway (af-south-1, proprietary)
  • Validates token via ona-user-auth Lambda
  • Checks nehanda subscription tier + quota
  • Forwards compacted payload to vLLM
          │
          ▼
  Nehanda 27B on EC2 (g6e.12xlarge, closed)
```

## Quick start

```bash
# Install
git clone https://github.com/AsobaCloud/nehanda-cli.git
cd nehanda-cli
./install.sh

# Authenticate (device flow — opens browser)
nehanda auth login

# Start a session
nehanda
```

## Architecture

See [docs/ARCHITECTURE.md](docs/ARCHITECTURE.md) for the full three-tier design, AGPL boundary analysis, and auth flow.

## Self-hosting

See [docs/SELF_HOST.md](docs/SELF_HOST.md) for running entirely locally without the Nehanda Gateway (bring your own model endpoint).

## License

AGPL-3.0. See [LICENSE](LICENSE).

This project is a fork of [aimee](https://github.com/RakuenSoftware/aimee) (AGPL-3.0, Copyright © 2026 The aimee authors). See [NOTICE](NOTICE) for third-party attributions.

## Upstream

This repo tracks `RakuenSoftware/aimee` as a git subtree under `upstream/`. To pull upstream changes:

```bash
git fetch aimee-upstream
git subtree pull --prefix=upstream aimee-upstream main --squash
```
