# nehanda-cli

A local AI coding substrate powered by the [**Nehanda** fine-tuned model](https://huggingface.co/asoba/nehanda-v3-27b). Free and open-source (AGPL-3.0).

Built on [aimee](https://github.com/RakuenSoftware/aimee) — local memory substrate with 4-tier context compaction, supervisor/delegate task decomposition, and guardrails. This fork wires Nehanda in as the primary reasoning model: a fine-tuned Qwen3.6 27B multimodal stack (technical writing, deep research, coding, document review, vision).

```
  Your Terminal
    └─ aichat (TUI) ──────────────────────────→ nehanda-server :8740 → nehanda-server (UDS)
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

`install.sh` builds the nehanda stack and installs [aichat](https://github.com/sigoden/aichat) as the interactive TUI. See [docs/QUICK_START.md](docs/QUICK_START.md) for full setup and verification.

### Automatic Workspace Registration

When you run `nehanda` from any git repository, it automatically adds that directory as a workspace. This enables the agent to access files from your actual working directory instead of being sandboxed in worktrees. The workspace registration wrapper is installed automatically during the setup process.

#### Working Directory Propagation

The nehanda TUI (`nehanda-ui`) passes the current working directory (cwd) to the server on every request. This ensures that file operations and tool execution happen in the correct directory — the one where you invoked `nehanda`, not the nehanda-cli installation directory.

**How it works:**
1. The workspace wrapper (`~/.local/bin/nehanda`) captures `$(pwd)` when invoked
2. `nehanda-ui` includes `cwd: process.cwd()` in every chat request
3. The server uses `run_cmd_set_cwd()` to set a thread-local working directory for tool execution
4. File read/write, bash commands, and other tools operate in the correct directory

This propagation is critical for multi-project workflows where you may invoke `nehanda` from different repositories.

### Model Configuration

Nehanda-cli supports any OpenAI-compatible model as your primary agent or orchestrator. You can configure models through the agent configuration system, allowing you to use models from various providers (including ZAI, OpenAI, or self-hosted models) without being locked to a specific provider. See [docs/QUICK_START.md](docs/QUICK_START.md) for model configuration details.

## Architecture

See [docs/ARCHITECTURE.md](docs/ARCHITECTURE.md) for the three-tier design and AGPL boundary analysis.

## Refactor plan

See [docs/REFACTOR_PLAN.md](docs/REFACTOR_PLAN.md) for the phased roadmap. Phase 0 (full native stack smoke test) passed on macOS 2026-07-12.

## Self-hosting

See [docs/SELF_HOST.md](docs/SELF_HOST.md) for running with a local model instead of the EC2 endpoint.

## License

AGPL-3.0. See [LICENSE](LICENSE) and [LICENSE.md](LICENSE.md).

This project is a fork of [aimee](https://github.com/RakuenSoftware/aimee) (AGPL-3.0, Copyright © 2026 The aimee authors). See [NOTICE](NOTICE) for third-party attributions.

## Security

See [SECURITY.md](SECURITY.md) for how to report vulnerabilities.

## Upstream

This repo tracks `RakuenSoftware/aimee` as a git subtree under `upstream/`. To pull upstream changes:

```bash
git fetch aimee-upstream
git subtree pull --prefix=upstream aimee-upstream main --squash
```
