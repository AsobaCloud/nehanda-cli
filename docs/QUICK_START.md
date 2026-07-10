# Quick Start

## Requirements

- macOS or Linux
- cmake 3.16+, make, git, C11 compiler
- Docker (for the local KB inference container)
- Ollama (optional — for local delegate workers)

## Install

```bash
git clone https://github.com/AsobaCloud/nehanda-cli.git
cd nehanda-cli
./install.sh
```

Add to your shell profile if not already there:
```bash
export PATH="$HOME/.local/bin:$PATH"
```

## Authenticate

```bash
nehanda auth login
```

This opens a browser to complete sign-in and payment. Your API key is stored locally in the session database — you never need to handle it manually.

## Start Docker services

The local KB inference container handles embeddings and session synthesis locally:

```bash
cd nehanda-cli
docker compose -f upstream/compose.combined.yaml up -d
```

Wait for it to be healthy:
```bash
curl -k -H 'Authorization: Bearer nehanda-local-dev' https://localhost:8743/v1/health
```

## Start a session

```bash
nehanda
```

This drops into the nehanda TUI with Nehanda 27B as the primary model.

## Use with your existing CLI tool

### Claude Code
```bash
./upstream/configure-hooks.sh
nehanda claude-proxy enable http://127.0.0.1:8910 nehanda-local-dev
ANTHROPIC_BASE_URL=http://127.0.0.1:8910 ANTHROPIC_AUTH_TOKEN=nehanda-local-dev claude
```

### ona-code
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

## Register local Ollama delegates (optional)

```bash
export AIMEE_SERVER_URL=https://localhost:8743
export AIMEE_SERVER_TOKEN=nehanda-local-dev

nehanda agent local ollama-local http://localhost:11434/v1 \
  --model llama3:latest \
  --slots 2 \
  --ctx 16384
```

## Index your project

```bash
nehanda workspace add ~/Workbench/your-project
```
