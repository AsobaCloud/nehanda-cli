# Troubleshooting

## `aimee chat: no final response from server`

The TUI connects but gets no response. Run the smoke test to find the actual failing step:

```bash
bash scripts/phase0-smoke.sh
```

All steps 0.1–0.9 must pass. A passing client connection with failing KB (step 0.7) is a failed install — not a partial success.

---

## Root causes found during Phase 0 (2026-07-12)

### 1. Stale `remote.conf` routing chat away from local server

**Symptom:** Chat returns no response even though `nehanda-server` is running. Server logs show no activity during a turn attempt.

**Cause:** If a Docker-based install was ever run, it wrote `~/.config/aimee/remote.conf` pointing at `https://localhost:8743`. The native stack uses a Unix domain socket. When `remote.conf` exists, the client ignores the UDS and tries the stale TLS endpoint instead.

**Fix:**
```bash
mv ~/.config/aimee/remote.conf ~/.config/aimee/remote.conf.bak
```

The smoke script does this automatically. `install.sh` (Phase 2) will not write `remote.conf`.

---

### 2. `--key none` fails when registering the EC2 agent

**Symptom:** `nehanda agent add ... --key none` errors with a vault or key storage failure.

**Cause:** `--key none` tries to vault-store the literal string `"none"`. The vault requires auth to write.

**Fix:** Omit `--key` entirely for the open EC2 vLLM endpoint:
```bash
nehanda agent add nehanda http://nehanda.asoba.co:8000 nehanda-rag-synthesis-27b \
  --provider openai \
  --no-tools \
  --roles "code,review,explain,refactor,draft,execute,summarize,plan,validate" \
  --default
nehanda config set provider nehanda
```

---

### 3. EC2 vLLM rejects tool calls — `--no-tools` required

**Symptom:** Chat returns an error about `tool_choice` not being supported, or the turn returns empty.

**Cause:** The provider chat path defaults to sending OpenAI tool payloads. Nehanda 27B on vLLM does not support `tool_choice: auto`.

**Fix:** Always register the EC2 agent with `--no-tools` (see above). Also write `tools_enabled: false` explicitly into `agents.json`:
```bash
python3 - <<'PY'
import json, os
path = os.path.expanduser("~/.config/aimee/agents.json")
data = json.load(open(path))
for a in data.get("agents", []):
    if a.get("name") == "nehanda":
        a["tools_enabled"] = False
json.dump(data, open(path, "w"), indent=2)
print("done")
PY
```

This is needed because the loader re-derives `tools_enabled: true` from model capabilities when the key is absent, overriding the `--no-tools` flag at load time.

---

### 4. `nehanda-kb` crashes with SIGBUS on macOS

**Symptom:** `nehanda-kb` crashes immediately. `~/Library/Logs/DiagnosticReports/nehanda-kb-*.ips` is created. `phase0-smoke.sh` step 0.4 fails.

**Cause:** `config_t` is ~722 KB. macOS default pthread stack is 512 KB. Background threads in kb (curator drain, reflection, mining, HTTP listener) that put `config_t` on the stack overflow on first function call.

**Fix:** Applied in `patches/002-macos-native-build.patch` — background threads heap-allocate `config_t`, all worker/listener threads use 32 MB stacks. Applied automatically by `install.sh` and `phase0-smoke.sh`. If you built manually without the patch:
```bash
git -C upstream apply patches/002-macos-native-build.patch
make -C upstream/src ../aimee-kb
cp upstream/aimee-kb ~/.local/bin/nehanda-kb
```

---

## Service health checks

```bash
# Embedder
curl -sf 'http://127.0.0.1:8742/health'

# KB
curl -sf 'http://127.0.0.1:8741/v1/health?status=1'

# Server (Unix domain socket)
curl --unix-socket ~/.config/aimee/aimee-http.sock http://localhost/v1/health

# KB status through server
curl --unix-socket ~/.config/aimee/aimee-http.sock http://localhost/v1/kb/status
# "available":true = KB is working; "available":false = install failed
```

---

## Re-register agents after a server restart

`nehanda-server` stores agent config in `~/.config/aimee/agents.json`. This persists across restarts. But if the file is deleted or the server home changes, re-register:

```bash
# Primary agent
nehanda agent add nehanda http://nehanda.asoba.co:8000 nehanda-rag-synthesis-27b \
  --provider openai --no-tools \
  --roles "code,review,explain,refactor,draft,execute,summarize,plan,validate" \
  --default
nehanda config set provider nehanda

# Windows LAN delegates (if reachable)
nehanda agent local ollama-remote-coder http://AsobaCorp-1.local:11434/v1 \
  --model deepseek-coder-v2:latest --slots 2 --ctx 32768
nehanda agent local ollama-remote-qwen http://AsobaCorp-1.local:11434/v1 \
  --model qwen2.5:14b --slots 2 --ctx 32768
nehanda agent local ollama-remote-reasoner http://AsobaCorp-1.local:11434/v1 \
  --model deepseek-r1:14b --slots 1 --ctx 32768
```

---

## Full recovery sequence

```bash
# 1. Kill any stale processes
pkill -x nehanda-server nehanda-kb 2>/dev/null; pkill -f start-embedder.sh 2>/dev/null

# 2. Clear stale remote.conf if present
[ -f ~/.config/aimee/remote.conf ] && mv ~/.config/aimee/remote.conf ~/.config/aimee/remote.conf.bak

# 3. Re-run the smoke test (starts fresh stack + registers agents)
bash scripts/phase0-smoke.sh

# 4. Reload shell
source ~/.zshrc
```
