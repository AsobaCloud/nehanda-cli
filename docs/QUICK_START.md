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

`install.sh` does everything: builds the binary, starts the Docker stack, trusts the server TLS cert, rotates the bearer token, and registers Nehanda as the primary agent. On completion it prints the `export` lines to add to your shell profile.

Add to your shell profile if not already there:
```bash
export PATH="$HOME/.local/bin:$PATH"
export AIMEE_SERVER_URL=https://localhost:8743
export AIMEE_SERVER_TOKEN=<token printed by install.sh>
```

## Start a session

```bash
nehanda
```

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
