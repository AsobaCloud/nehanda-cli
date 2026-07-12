# nehanda-cli

A local AI coding substrate powered by the [**Nehanda** fine-tuned model](https://huggingface.co/asoba/nehanda-v3-27b). Free and open-source (AGPL-3.0).

Built on [aimee](https://github.com/RakuenSoftware/aimee) — local memory substrate with 4-tier context compaction, supervisor/delegate task decomposition, and guardrails. This fork wires Nehanda in as the primary reasoning model: a fine-tuned Qwen3.6 27B multimodal stack (technical writing, deep research, coding, document review, vision).

```
  Your Terminal (nehanda TUI)
          │  Unix domain socket
          ▼
  nehanda-server  (local, AGPL)
  • 4-tier memory compaction (~86% token reduction pre-egress)
  • Code index + KB — postgres + pgvector, never leaves machine
  • Supervisor/Delegate task fan-out → local/LAN Ollama (free)
          │  compacted payload only
          ▼
  Nehanda 27B on EC2 (nehanda.asoba.co:8000)
  • vLLM, tensor-parallel
  • nehanda-rag-synthesis-27b
```

**No Docker. Three native binaries + postgres + local embedder.**

## Quick start

```bash
git clone https://github.com/AsobaCloud/nehanda-cli.git
cd nehanda-cli
./install.sh
nehanda
```

See [docs/QUICK_START.md](docs/QUICK_START.md) for full setup and verification.

## Architecture

See [docs/ARCHITECTURE.md](docs/ARCHITECTURE.md) for the three-tier design and AGPL boundary analysis.

## Refactor plan

See [docs/REFACTOR_PLAN.md](docs/REFACTOR_PLAN.md) for the phased roadmap. Phase 0 (full native stack smoke test) passed on macOS 2026-07-12.

## Self-hosting

See [docs/SELF_HOST.md](docs/SELF_HOST.md) for running with a local model instead of the EC2 endpoint.

## License

AGPL-3.0. See [LICENSE](LICENSE).

This project is a fork of [aimee](https://github.com/RakuenSoftware/aimee) (AGPL-3.0, Copyright © 2026 The aimee authors). See [NOTICE](NOTICE) for third-party attributions.

## Upstream

This repo tracks `RakuenSoftware/aimee` as a git subtree under `upstream/`. To pull upstream changes:

```bash
git fetch aimee-upstream
git subtree pull --prefix=upstream aimee-upstream main --squash
```
