# Testing ona-code

## Behavioral testing

Tests must exercise production entry points and observable outcomes. External model/controller replies may be mocked; copying production filters, fabricating execution results or assigning a database phase as proof of a transition does not establish behavior. Fixtures may seed the starting state. Assertions must verify real gate, tool, filesystem, database and subprocess effects.

## Acceptance and task tests

| System | Purpose | Entry point |
| --- | --- | --- |
| Issue #36/#37 acceptance | Canonical lifecycle, execution provenance, exploration/Jev integration and CLI completion | `node tests/integration/run-acceptance.mjs --issues 36,37` |
| Unit/regression suites | Existing module/provider/UI contracts; runner continues after RED | `npm run test:unit` |
| Legacy black-box checks | Original clean-room CLI surface | `npm run acceptance` |
| Task test creation | Write and execute a diagnosed behavioral regression to RED before plan approval | `create_tests` |
| Task verification | Original unchanged regression must pass against current source | `verify` |
| Task safety | Actual named checkers and current measured branch coverage ≥70% | `safety` |

The legacy black-box script includes older clean-room expectations; the canonical issue acceptance manifest is authoritative for the new lifecycle. Legacy failures must be reported separately rather than used to restore obsolete phase semantics.

## Running the scoped contract

Use Node 22 matching the installed native SQLite module:

```bash
node tests/integration/run-acceptance.mjs --issues 36,37 --syntax
node tests/integration/run-acceptance.mjs --issues 36,37 --json /tmp/acceptance.json
```

Use `--transport fetch` when localhost sockets are restricted. This mocks only the external provider Request/Response boundary. It retains real stream parsing, tools, SQLite/Jev, gates, tests, coverage and checkers. Socket mode remains the default and must be tested separately on a permitted host. Real Ollama D1/D2/D3 require `OLLAMA_E2E_TEST=1`; access failures do not count as passes.

The scoped runner continues after failures, preserves source hashes and raw logs, and distinguishes PASS, assertion RED, HARNESS_ERROR, BLOCKED and optional NOT_RUN. A blocked case cannot satisfy acceptance. The complete Given/When/Then matrix is in [`../tests/acceptance-36-37.md`](../tests/acceptance-36-37.md).

## Safety checks

```bash
node lib/scripts/js-safety-checker.cjs .
bash lib/scripts/shell-safety-checker.sh .
python3 lib/scripts/python-safety-checker.py .
```

Inspect examined-file counts and diagnostics. Test/source/report identities must match the actual executions; a numeric exit code or hand-authored coverage percentage alone cannot authorize completion.

## Legacy black-box architecture

The Bash test driver launches the CLI against a Python HTTP server emitting SSE, then inspects the temporary SQLite database. The driver verifies persisted tool results, hooks, phase transitions and transcript entries through direct database assertions. The following helpers and row inventory describe that legacy harness.

## Mock server protocol

The mock server parses the last user message for directives:

| Directive | Mock response | Use case |
|-----------|--------------|----------|
| `__TOOL__:Read:{"file_path":"/tmp/x"}` | Returns an SSE tool_call for `Read` with those args | Test tool execution |
| `__ASSIST_TEXT__:hello world` | Returns SSE text content `hello world` | Test transcript persistence |
| *(no directive)* | Returns `(mock) no directive` | Default behavior |

The mock returns proper SSE streaming chunks (not plain JSON), matching what LM Studio and OpenAI return. This tests the full streaming parser path.

## Helper functions

### `fresh_db <name>`

Creates a fresh temporary SQLite database via `ona --init-db`. Sets `AGENT_SDLC_DB` to the new path. Each test gets its own DB for isolation.

### `db <sql>`

Runs a SQLite query against `$AGENT_SDLC_DB` with the required pragmas (foreign_keys, busy_timeout). Use for all DB assertions.

### `tool_one <row> <tool_name> <args_json>`

The key helper for tool tests. Creates a fresh DB seeded in `implement` phase with an approved plan (so the planning gate allows mutating tools), then runs `ona --eval` with the specified tool. Returns the JSON result directly.

```bash
pj=$(tool_one 60 Read '{"file_path":"/tmp/test.txt"}')
echo "$pj" | grep -q '"is_error":false'
```

### `write_mock_server <port>`

Starts the Python mock LLM server on the specified port. Sets `MOCK_PID` for cleanup.

### `put_effective_json <json>`

Seeds the `settings_snapshot` table with the given JSON as the `effective` scope. Used to configure hooks, permissions, and model settings for specific tests.

## Adding a test row

1. Choose a row ID following the convention: `ROW-XX` where XX groups by spec section (01-08: storage, 10-14: providers, 20-25: auth, 30-38: REPL, 40-49: hooks, 50-53: permissions, 60-86: tools, 90-96: workflow, 100-102: forbidden, 110-112: payloads).

2. Write a function:

```bash
row_XX() {
  fresh_db rowXX
  # Setup: create fixtures, seed DB
  # Exercise: pipe input to ona or use tool_one
  # Assert: query DB with db(), grep output, check files
}
run_check ROW-XX row_XX
```

3. The function must return 0 on pass, non-zero on fail. Use `grep -q` for assertions, `test` for comparisons.

4. Do NOT import Node modules. Do NOT call internal functions directly. If you can't test it from the outside, flag it — don't fake it.

## Row inventory (34 rows)

| Group | Rows | Count |
|-------|------|-------|
| DDL/Schema | ROW-01 to ROW-05 | 5 |
| REPL commands | ROW-30 to ROW-38 | 6 |
| Hooks | ROW-40, ROW-49 | 2 |
| Permissions | ROW-50 to ROW-52 | 3 |
| Tools | ROW-60 to ROW-85 | 10 |
| Workflow | ROW-91, ROW-92, ROW-95 | 3 |
| Forbidden patterns | ROW-100 | 1 |
| Payloads | ROW-110, ROW-111 | 2 |
| **Total** | | **34** |

## What's NOT tested yet

The following spec sections have coverage gaps:

- Providers (ROW-10 to ROW-14): live provider tests with mock server
- Auth capabilities (ROW-20 to ROW-25): env var auth, logout, secret hiding, bare mode
- Hook stdin fields (ROW-46 to ROW-48): stdin JSON shape, newline, env vars
- Hook behavior (ROW-41 to ROW-45): sequential ordinals, exit 2 blocking, timeout, permission merge
- More tools (ROW-69 to ROW-82): Bash stderr/truncation, NotebookEdit, WebFetch, WebSearch, ExitPlanMode, AskUserQuestion, Brief, TodoWrite, TaskOutput, TaskStop, Agent
- Workflow gates (ROW-93, ROW-94, ROW-96): test->verify, planning requires plan, non-mutating allowed

These should be added incrementally. Each is straightforward using the existing helpers.
