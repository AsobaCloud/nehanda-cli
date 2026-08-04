# Nehanda CLI (`nehanda-cli`)

An agentic terminal REPL and single-process engine built for governed AI software development. Powered by the native `ona-code` core, `nehanda-cli` connects directly to local or remote endpoints—including Nehanda's fine-tuned 27B model on AWS SageMaker—without external daemons, cross-repository dependencies, or middleware servers.

Every conversation turn, tool call, phase transition, and permission check is stored in a local, queryable SQLite database you own.

---

## Key Features

- **In-Process Engine:** Executes turns directly inside the process via `runUserTurn`, eliminating separate server daemons or background HTTP relays.
- **Dynamic XML Tool Rescue:** Native support for endpoints that strip OpenAI tool schemas (such as `nehandaMlProxy`). The engine dynamically injects active tool schemas directly into system prompts as XML contracts (`<tool_call>`), parsing and executing tools locally without cloud-side function calling support.
- **Deterministic SDLC Workflow:** Enforces a 6-phase state machine (`idle` → `plan` → `implement` → `test` → `verify` → `done`) to prevent unapproved code changes, hallucinated test passes, or unverified implementations.
- **Interactive TUI & Pipe Support:** Rich Ink-based TUI (`bin/nehanda-ui.mjs`) for interactive development sessions, with headless pipe-mode support (`bin/agent.mjs`) for acceptance testing and automation.
- **Multi-Provider Switching:** Seamlessly switch between Nehanda 27B, local LM Studio, Ollama instances over LAN, Anthropic Claude, or any OpenAI-compatible API using `/model`.

---

## Architecture

`nehanda-cli` combines an Ink TUI with `ona-code`'s deterministic orchestration engine:


```

[bin/nehanda-ui.mjs] (TUI Layer)
│

│ (1) bridge.onSubmit -> runUserTurn(db, rt, text, io)
▼
[lib/orchestrate.mjs] (In-Process Engine)
│

│ (2) Checks omitToolChoice(provider)
│ (3) Formats registered tools into system prompt XML instructions
│ (4) Calls streamOpenAIChatCompletion({ omitToolChoice: true })
▼
[nehandaMlProxy / Endpoint]
│

│ (5) Model returns streamed plain-text response
▼
[lib/orchestrate.mjs]
│

│ (6) parseXmlToolCalls(fullText) rescues <tool_call> XML tags
│ (7) Executes local shell/file tools & logs tool_result rows
▼
[~/.config/nehanda/ona-session.db] (SQLite State)

```

---

## Getting Started

### Requirements
- **Node.js**: v22.0.0 or higher
- **SQLite**: Local SQLite runtime support

### 1. Installation

```bash
git clone [https://github.com/AsobaCloud/nehanda-cli.git](https://github.com/AsobaCloud/nehanda-cli.git)
cd nehanda-cli
npm install

```

### 2. Launching the REPL

Launch the interactive Ink TUI:

```bash
npm start
# or directly run:
node bin/nehanda-ui.mjs

```

### 3. Provider Setup

#### Option A: Nehanda Cloud (Default)

The CLI defaults to the primary Nehanda endpoint (`https://nehanda-ml.asoba.co/v1`). If an API key is required:

```
❯ /key
New Nehanda API key: <your-key>

```

#### Option B: Local LM Studio

Start LM Studio locally on port `1234` or `8000`, then start the CLI. Select or switch models via:

```
❯ /model

```

#### Option C: Remote Ollama over LAN

To connect to an Ollama instance running on your network:

```
❯ /config base_url [http://AsobaCorp-1.local:11434/v1](http://AsobaCorp-1.local:11434/v1)
❯ /model ollama/deepseek-coder-v2:latest

```

---

## REPL Commands

| Command | Description |
| --- | --- |
| `/help` | Display available commands |
| `/model [name]` | Discover and switch active provider or model endpoint |
| `/key` | Save Nehanda API key |
| `/config` | View or set settings (e.g. `/config base_url <url>`) |
| `/clear` | Clear conversation history and reset transcript state |
| `/retry` | Resend the last failed request |
| `/exit` | Exit the REPL |

---

## SDLC Workflow

The engine enforces state transitions across six distinct phases:

1. **`idle`**: Discovery and triage. Mutating file tools are physically masked out.
2. **`plan`**: Model formulates success criteria and implementation steps (`EnterPlanMode`).
3. **`implement`**: Code changes applied using file editing and shell execution (`ExitPlanMode`).
4. **`test`**: Automated test generation and execution (`SubmitImplementation`).
5. **`verify`**: Inspection of test outputs and coverage verification (`SubmitTest`).
6. **`done`**: Final sign-off and git commit creation.

---

## Database Schema

Session state is persisted locally at `~/.config/nehanda/ona-session.db`. Key tables include:

* `conversations`: Active workflow phases and project roots.
* `transcript_entries`: Sequence of user messages, assistant turns, tool calls, and results.
* `plans`: Content, hashes, and approval status for technical plans.
* `events`: SDLC milestones and test execution output.

---

## Testing & Verification

Run the acceptance suite:

```bash
npm run acceptance

```

Verify SDLC hook ordering:

```bash
npm run verify

```

---

## License

See [LICENSE](https://www.google.com/search?q=LICENSE) for details.

