# Troubleshooting

## `aimee chat: no final response from server`

The nehanda TUI connects but gets no response. Usually means the server-side stack is unhealthy.

**Check container status first:**
```bash
docker ps --format "table {{.Names}}\t{{.Status}}"
```

---

## Containers show `(unhealthy)` — `exec /bin/sh: input/output error`

**Symptom:** Both `upstream-aimee-server-kb-1` and `upstream-postgres-1` show `Up N hours (unhealthy)`. Docker container logs are unreadable. Health checks fail with `exec /bin/sh: input/output error`.

**Cause:** Docker Desktop's Linux VM filesystem has become corrupted or its disk is full. The container processes are still running (the server will respond to network requests) but Docker can no longer exec into the containers or run health check commands.

**Diagnosis:**
```bash
docker inspect upstream-aimee-server-kb-1 --format '{{json .State.Health}}' \
  | python3 -c "import sys,json; h=json.load(sys.stdin); [print(l['Output']) for l in h.get('Log',[])]"
# If output is: exec /bin/sh: input/output error → Docker VM is the problem
```

**Fix:** Restart Docker Desktop fully — quit the application entirely (not just the containers) and relaunch it. This resets the VM.

After Docker Desktop comes back up, the containers auto-restart. Verify:
```bash
docker ps --format "table {{.Names}}\t{{.Status}}"
# Both should show (healthy) within ~30 seconds
```

---

## Bearer token is missing or invalid after restart

**Symptom:** `nehanda` can't connect after the Docker stack restarts. `~/.config/aimee/nehanda-install-token` is empty or missing. Server returns `{"error":{"message":"missing or invalid bearer token"}}`.

**Cause:** The bootstrap bearer token `aimee-local-dev` can only be rotated once. After a fresh container start it becomes available again, but the stored token file may have been lost.

**Fix:**
```bash
# 1. Rotate the bootstrap token (works on a fresh container start)
TOKEN=$(curl -sk -X POST \
  -H 'Authorization: Bearer aimee-local-dev' \
  https://localhost:8743/v1/api/rotate_bearer \
  | python3 -c "import sys,json; print(json.load(sys.stdin)['bearer_token'])")

# 2. Store it
echo "$TOKEN" > ~/.config/aimee/nehanda-install-token
chmod 600 ~/.config/aimee/nehanda-install-token

# 3. Wire the client
nehanda remote set https://localhost:8743 "$TOKEN"

# 4. Update shell profile
sed -i '' "s|export AIMEE_SERVER_TOKEN=.*|export AIMEE_SERVER_TOKEN=$TOKEN|" ~/.zshrc \
  || echo "export AIMEE_SERVER_TOKEN=$TOKEN" >> ~/.zshrc
```

---

## Re-register agents after a full stack restart

A full Docker restart resets the database. Nehanda and delegates need to be re-registered:

```bash
# Primary agent
nehanda agent add nehanda http://nehanda.asoba.co:8000 nehanda-rag-synthesis-27b \
  --provider openai --key "none" \
  --roles "code,review,explain,refactor,draft,execute,summarize,plan,validate,diagnose" \
  --default
nehanda config set provider nehanda

# Windows LAN delegates
nehanda agent local ollama-remote-coder http://AsobaCorp-1.local:11434/v1 \
  --model deepseek-coder-v2:latest --slots 2 --ctx 32768
nehanda agent local ollama-remote-qwen http://AsobaCorp-1.local:11434/v1 \
  --model qwen2.5:14b --slots 2 --ctx 32768
nehanda agent local ollama-remote-reasoner http://AsobaCorp-1.local:11434/v1 \
  --model deepseek-r1:14b --slots 1 --ctx 32768
nehanda agent local ollama-remote-gemma http://AsobaCorp-1.local:11434/v1 \
  --model codegemma:7b --slots 2 --ctx 16384
```

Verify everything is back:
```bash
nehanda agent list
nehanda agent probe nehanda
```

---

## Full recovery sequence (after Docker Desktop restart)

```bash
# 1. Confirm containers healthy
docker ps --format "table {{.Names}}\t{{.Status}}"

# 2. Rotate token and wire client
TOKEN=$(curl -sk -X POST -H 'Authorization: Bearer aimee-local-dev' \
  https://localhost:8743/v1/api/rotate_bearer \
  | python3 -c "import sys,json; print(json.load(sys.stdin)['bearer_token'])")
echo "$TOKEN" > ~/.config/aimee/nehanda-install-token
chmod 600 ~/.config/aimee/nehanda-install-token
nehanda remote set https://localhost:8743 "$TOKEN"
sed -i '' "s|export AIMEE_SERVER_TOKEN=.*|export AIMEE_SERVER_TOKEN=$TOKEN|" ~/.zshrc \
  || echo "export AIMEE_SERVER_TOKEN=$TOKEN" >> ~/.zshrc

# 3. Re-register agents
nehanda agent add nehanda http://nehanda.asoba.co:8000 nehanda-rag-synthesis-27b \
  --provider openai --key "none" --roles "code,review,explain,refactor,draft,execute,summarize,plan,validate,diagnose" --default
nehanda config set provider nehanda
nehanda agent local ollama-remote-coder http://AsobaCorp-1.local:11434/v1 --model deepseek-coder-v2:latest --slots 2 --ctx 32768
nehanda agent local ollama-remote-qwen http://AsobaCorp-1.local:11434/v1 --model qwen2.5:14b --slots 2 --ctx 32768
nehanda agent local ollama-remote-reasoner http://AsobaCorp-1.local:11434/v1 --model deepseek-r1:14b --slots 1 --ctx 32768
nehanda agent local ollama-remote-gemma http://AsobaCorp-1.local:11434/v1 --model codegemma:7b --slots 2 --ctx 16384

# 4. Verify
nehanda agent list
nehanda
```
