# Quick Start

## Requirements

- macOS or Linux
- cmake 3.16+, make, git, C11 compiler
- Homebrew (macOS) or apt/dnf (Linux)
- Ollama (optional — for local and LAN delegate workers)

**No Docker required.** `install.sh` handles postgres, pgvector, and all build dependencies.

## Install

```bash
git clone https://github.com/AsobaCloud/nehanda-cli.git
cd nehanda-cli
./install.sh
```

Builds three binaries (`nehanda`, `nehanda-server`, `nehanda-kb`), installs postgres + pgvector, starts local services, installs the Nehanda system prompt, registers the EC2 agent, and verifies chat — one command, no follow-up steps.

```bash
./install.sh    # ~40s on re-run with cached build; first cold compile takes longer
nehanda         # interactive TUI (nehanda-ui, configured against nehanda-server)
nehanda chat "…"  # one-shot message (server stream, no TUI)
```

Add to your shell profile:
```bash
export PATH="$HOME/.local/bin:$PATH"
export PATH="/opt/homebrew/opt/postgresql@17/bin:$PATH"   # macOS only
```

## Start a session

```bash
nehanda
```

`nehanda` launches `nehanda-ui`, an Ink-based terminal UI that talks directly to nehanda-server over the loopback HTTP API (`127.0.0.1:8740`). The UI shows the current agent, model, endpoint, bearer token, and live token usage. Type `/help` for available commands.

Available slash commands in `nehanda-ui`:

| Command | Description |
|---|---|
| `/model` | Change aimee agent model or orchestrator model |
| `/bearer` | Update bearer token in `aimee.yaml` and aichat config atomically |
| `/token` | Last 24h token usage breakdown |
| `/auth` | Auth status / login stub |
| `/config` | Show current config |
| `/clear` | Start a new conversation |
| `/help` | List all commands |
| `/exit` | Quit |

`nehanda chat "message"` (one-shot, with inline text) sends directly to the server stream API without the TUI.

## Nehanda identity (system prompt)

`install.sh` installs Nehanda-specific prompts so the model identifies correctly:

| File | Installed to |
|---|---|
| `config/webchat_system_prompt.txt` | `~/.config/aimee/webchat_system_prompt.txt` |
| `config/personas/engineer.md` | `~/.config/aimee/personas/engineer.md` |

These override the upstream AIMEE engineer persona. Nehanda presents as a fine-tuned Qwen3.6 27B multimodal assistant with strengths in technical writing, deep research, coding, document review, and vision (images/diagrams the user provides).

To customize identity or capabilities, edit the files in `config/` and re-run `./install.sh`, or copy them into `~/.config/aimee/` directly. Start a new session after changes.

## Native services (optional persistence)

`install.sh` starts embedder, kb, and server for the current session. To install launchd agents that survive reboot:

```bash
bash scripts/install-native-services.sh
bash scripts/start-native-services.sh status   # embedder, kb, server all ok
```

## How the stack works

```
nehanda (interactive)
  └── nehanda-ui (Ink TUI) ── HTTP 127.0.0.1:8740/v1 ──→ nehanda-server (UDS)
        ├── nehanda-kb  (127.0.0.1:8741)
        │       ├── postgres aimee_shared  (vectors, code graph, KB docs)
        │       └── embedder  (127.0.0.1:8742 — local CPU, Qwen3-0.6B)
        └── nehanda.asoba.co:8000  (EC2 — compacted chat payload only)
```

KB, memory, embeddings, and code index never leave the machine. Only the final compacted prompt goes to EC2.

## Verify the full stack

```bash
bash scripts/phase0-smoke.sh
```

Steps 0.1–0.9 must all pass. A passing client connection with failing KB (step 0.7) is a failed install.

## Register Ollama delegates (optional)

```bash
# Local Mac
nehanda agent local ollama-local http://localhost:11434/v1 \
  --model llama3:latest --slots 2 --ctx 16384

# Remote LAN (Windows machine)
nehanda agent local ollama-remote-coder http://AsobaCorp-1.local:11434/v1 \
  --model deepseek-coder-v2:latest --slots 2 --ctx 32768
```

## Index your project

```bash
nehanda workspace add ~/Workbench/your-project
curl -sf 'http://127.0.0.1:8741/v1/health?status=1' | grep '"available":true'
```
