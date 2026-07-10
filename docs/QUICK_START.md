# Quick Start

## Requirements

- macOS or Linux
- cmake 3.16+, make, git, C11 compiler
- Docker (for the server stack)
- Ollama (optional — for local delegate workers)

## Install

```bash
git clone https://github.com/AsobaCloud/nehanda-cli.git
cd nehanda-cli
./install.sh
```

`install.sh` builds the binary, starts the Docker stack, and trusts the server's TLS cert in the macOS keychain (requires your password once).

Add to your shell profile if not already there:
```bash
export PATH="$HOME/.local/bin:$PATH"
export AIMEE_SERVER_URL=https://localhost:8743
export AIMEE_SERVER_TOKEN=<your-token>   # from install output
```

## Start the Docker stack (if not already running)

```bash
cd nehanda-cli
docker compose -f upstream/compose.combined.yaml up -d
curl -sk -H "Authorization: Bearer $AIMEE_SERVER_TOKEN" https://localhost:8743/v1/health
# -> {"status":"ok","service":"aimee-server"}
```

## Register Nehanda as the primary model

```bash
nehanda agent add nehanda http://nehanda.asoba.co:8000 nehanda-rag-synthesis-27b \
  --provider openai --key "none" \
  --roles "code,review,explain,refactor,draft,execute,summarize,plan,validate,diagnose" \
  --default

nehanda config set provider nehanda
```

## Start a session

```bash
nehanda
```

This opens the nehanda TUI connected to Nehanda 27B. No external tools required, no vendor API keys, no cloud dependency beyond the Nehanda EC2 endpoint.

## Use with ona-code (optional — adds SDLC workflow enforcement)

ona-code connects to nehanda-server's OpenAI-compatible endpoint and adds a 6-phase state machine (plan → implement → test → verify) on top:

```bash
mkdir -p ~/.ona && cat > ~/.ona/settings.json << 'EOF'
{
  "model_config": {
    "provider": "openai_compatible",
    "model_id": "gpt_4o",
    "base_url": "https://localhost:8743/v1"
  }
}
EOF
cd /path/to/ona-code && npm start
```

## Register Ollama delegates (optional)

Local Mac delegates:
```bash
nehanda agent local ollama-local http://localhost:11434/v1 \
  --model llama3:latest --slots 2 --ctx 16384
```

Remote LAN delegates (Windows machine):
```bash
nehanda agent local ollama-remote-coder http://AsobaCorp-1.local:11434/v1 \
  --model deepseek-coder-v2:latest --slots 2 --ctx 32768
```

See `docs/SELF_HOST.md` for full delegate setup options.

## Index your project

```bash
nehanda workspace add ~/Workbench/your-project
```
