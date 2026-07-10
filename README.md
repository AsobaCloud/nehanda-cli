# nehanda-cli

A local AI coding substrate powered by the [**Nehanda** fine-tuned model](https://huggingface.co/asoba/nehanda-v3-27b). Free and open-source (AGPL-3.0).

Built on [aimee](https://github.com/RakuenSoftware/aimee) — a local memory substrate with 4-tier context compaction, supervisor/delegate task decomposition, and guardrails. This fork wires Nehanda in as the primary reasoning model, with your local and LAN Ollama instances as free delegate workers.

```
  Your Terminal (nehanda TUI, or any OpenAI-compatible CLI)
          │
          ▼
  nehanda-cli (local, AGPL)
  • 4-tier memory compaction (~86% token reduction)
  • Supervisor/Delegate task fan-out
  • Local Ollama delegates (free, zero egress)
          │  HTTPS — OpenAI wire protocol
          ▼
  Nehanda 27B on EC2 (nehanda.asoba.co:8000)
  • vLLM, tensor-parallel
  • nehanda-rag-synthesis-27b
```

## Quick start

```bash
git clone https://github.com/AsobaCloud/nehanda-cli.git
cd nehanda-cli
./install.sh
```

`install.sh` does everything end-to-end: builds the binary, starts the Docker stack, trusts the TLS cert, wires the client to the server, and registers Nehanda as the primary agent. On completion it prints the `export` lines to add to your shell profile.

```bash
# After install, start a session:
nehanda
```

See [docs/QUICK_START.md](docs/QUICK_START.md) for full setup details.

## Architecture

See [docs/ARCHITECTURE.md](docs/ARCHITECTURE.md) for the three-tier design and AGPL boundary analysis.

## Self-hosting

See [docs/SELF_HOST.md](docs/SELF_HOST.md) for running with a local model instead of the Nehanda EC2 endpoint.

## License

AGPL-3.0. See [LICENSE](LICENSE).

This project is a fork of [aimee](https://github.com/RakuenSoftware/aimee) (AGPL-3.0, Copyright © 2026 The aimee authors). See [NOTICE](NOTICE) for third-party attributions.

## Upstream

This repo tracks `RakuenSoftware/aimee` as a git subtree under `upstream/`. To pull upstream changes:

```bash
git fetch aimee-upstream
git subtree pull --prefix=upstream aimee-upstream main --squash
```
