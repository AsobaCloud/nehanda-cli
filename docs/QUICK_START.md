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

Builds three binaries (`nehanda`, `nehanda-server`, `nehanda-kb`), installs postgres + pgvector, starts local services, and registers Nehanda as the primary agent.

Add to your shell profile:
```bash
export PATH="$HOME/.local/bin:$PATH"
export PATH="/opt/homebrew/opt/postgresql@17/bin:$PATH"   # macOS only
```

## Start a session

```bash
nehanda
```

## How the stack works

```
nehanda (TUI)
  │  Unix domain socket
  ▼
nehanda-server  (~/.config/aimee/aimee-http.sock)
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
