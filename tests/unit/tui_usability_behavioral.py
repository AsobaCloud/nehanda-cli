#!/usr/bin/env python3
"""Issue #25: actual CLI/PTY outcomes; only provider and Laya are simulated."""

import contextlib
import http.server
import json
import os
import pty
import re
import select
import shutil
import sqlite3
import subprocess
import sys
import tempfile
import threading
import time
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
NODE = shutil.which('node')
ANSI = re.compile(r'\x1b\[[0-?]*[ -/]*[@-~]')


class Provider(http.server.BaseHTTPRequestHandler):
    def log_message(self, *_args):
        pass

    def do_GET(self):
        self.send_response(200)
        self.send_header('Content-Type', 'application/json')
        self.end_headers()
        self.wfile.write(json.dumps({'data': [{
            'id': 'energy-review-model', 'object': 'model', 'owned_by': 'test-provider',
        }]}).encode())

    def do_POST(self):
        body = json.loads(self.rfile.read(int(self.headers['Content-Length'])))
        self.server.requests.append((self.headers.get('Authorization'), body))
        self.send_response(200)
        self.send_header('Content-Type', 'text/event-stream')
        self.end_headers()
        try:
            if self.server.mode in ('table', 'sandbox', 'sandbox_slow') and len(self.server.requests) == 1:
                rows = [{'asset_id': f'inverter-{i}', 'power_kw': 42 + i} for i in range(12)]
                command = "printf '%s\\n' '" + json.dumps(rows) + "'"
                if self.server.mode.startswith('sandbox'):
                    command = ('killall nehanda-nonexistent-test-process-25 2>/dev/null || true; '
                               "printf 'sandbox-cli-executed\\n'; printf edited > telemetry.csv")
                    if self.server.mode == 'sandbox_slow':
                        command += '; sleep 30'
                self.chunk({'tool_calls': [{
                    'index': 0, 'id': 'telemetry-read', 'type': 'function',
                    'function': {'name': 'Bash', 'arguments': json.dumps({'command': command})},
                }]})
                self.wfile.write(b'data: [DONE]\n\n')
                self.wfile.flush()
                return
            if self.server.mode == 'reasoning':
                self.chunk({'reasoning_content': 'Compare inverter production against nameplate capacity'})
                time.sleep(0.5)
                self.chunk({'reasoning_content': ' Check the telemetry time window'})
                time.sleep(0.5)
            self.chunk({'content': 'Telemetry review finished.'})
            self.wfile.write(b'data: [DONE]\n\n')
            self.wfile.flush()
        except (BrokenPipeError, ConnectionResetError):
            pass

    def chunk(self, delta):
        data = {'choices': [{'index': 0, 'delta': delta, 'finish_reason': None}]}
        self.wfile.write(('data: ' + json.dumps(data) + '\n\n').encode())
        self.wfile.flush()


class Terminal:
    def __init__(self, home, endpoint, initialized=True, reuse=False):
        self.home = home
        self.endpoint = endpoint
        config = home / '.config' / 'nehanda'
        config.mkdir(parents=True, exist_ok=True)
        # Existing-session fixture uses the real canonical store initializer.
        # Fresh-install schema failure is a separate diagnosis, not evidence for
        # credential/progress/table regressions.
        module = (ROOT / 'lib' / 'store.mjs').as_uri()
        initializer = (
            'import {openStore,closeStore} from ' + json.dumps(module) + ';'
            'const p=' + json.dumps(str(config / 'ona-session.db')) + ';'
            'openStore(p);closeStore(p);'
        )
        if initialized:
            subprocess.run([NODE, '--input-type=module', '-e', initializer], check=True,
                           capture_output=True, text=True)
        (config / '.splash-seen').touch()
        if not reuse:
            (config / 'agents.json').write_text(json.dumps({
                'default_agent': 'test', 'agents': [{
                    'name': 'test', 'provider': 'openai_compatible',
                    'model': 'energy-review-model', 'endpoint': endpoint,
                    'api_key': 'dummy-nehanda-key',
                }],
            }))
        laya = home / 'laya'
        (laya / 'venv' / 'bin').mkdir(parents=True, exist_ok=True)
        (laya / '.laya-ready').touch()
        (laya / 'venv' / 'bin' / 'python').touch()
        worker = home / 'fake-laya'
        worker.write_text(
            '#!/usr/bin/env python3\nimport json, sys\n'
            'for line in sys.stdin:\n'
            '    msg = json.loads(line)\n'
            '    result = {"loaded": True} if msg["method"] == "load" else {}\n'
            '    print(json.dumps({"id": msg["id"], "ok": True, "result": result}), flush=True)\n'
        )
        worker.chmod(0o700)
        self.master, slave = pty.openpty()
        # HOME applies only to this child: never read or modify real user credentials.
        env = {k: v for k, v in os.environ.items() if k not in {
            'OPENAI_API_KEY', 'OPENROUTER_API_KEY', 'NEHANDA_API_KEY', 'ZAI_API_KEY',
        }}
        env.update({
            'HOME': str(home), 'ONAHOME': str(home / 'ona'),
            'NEHANDA_HOME': str(laya), 'LAYA_PYTHON': str(worker),
            'OPENROUTER_API_KEY': 'dummy-openrouter-key',
            'SDLC_DISABLE_ALL_HOOKS': '1', 'TERM': 'xterm-256color',
        })
        self.proc = subprocess.Popen(
            [NODE, str(ROOT / 'bin' / 'nehanda-ui.mjs')],
            cwd=home, env=env, stdin=slave, stdout=slave, stderr=slave,
            start_new_session=True,
        )
        os.close(slave)
        self.output = ''
        self.wait_for('Type /help for commands')

    def send(self, text):
        if len(text) > 1 and text.endswith('\r'):
            os.write(self.master, text[:-1].encode())
            self.pump()
            os.write(self.master, b'\r')
            return
        os.write(self.master, text.encode())

    def pump(self, seconds=0.15):
        deadline = time.monotonic() + seconds
        while time.monotonic() < deadline:
            ready, _, _ = select.select([self.master], [], [], 0.05)
            if ready:
                try:
                    self.output += os.read(self.master, 65536).decode(errors='replace')
                except OSError:
                    break
        return ANSI.sub('', self.output)

    def wait_for(self, text, seconds=8):
        deadline = time.monotonic() + seconds
        while time.monotonic() < deadline:
            if text in self.pump():
                return
        raise AssertionError(f'Terminal never showed {text!r}: {self.output[-2000:]!r}')

    def close(self):
        self.proc.terminate()
        try:
            self.proc.wait(timeout=3)
        except subprocess.TimeoutExpired:
            self.proc.kill()
            self.proc.wait()
        os.close(self.master)

    def turn(self, text):
        self.output = ''
        self.send(text + '\r')
        self.wait_for('Use SDLC workflow')
        self.send('n\r')
        self.wait_for('Telemetry review finished.')

    def restart(self):
        self.close()
        self.__init__(self.home, self.endpoint, reuse=True)


class TuiUsabilityBehavioral(unittest.TestCase):
    @contextlib.contextmanager
    def session(self, mode='normal', initialized=True):
        with tempfile.TemporaryDirectory(prefix='nehanda-tui-25-') as tmp:
            server = http.server.ThreadingHTTPServer(('127.0.0.1', 0), Provider)
            server.requests = []
            server.mode = mode
            thread = threading.Thread(target=server.serve_forever, daemon=True)
            thread.start()
            ui = None
            try:
                ui = Terminal(Path(tmp), f'http://127.0.0.1:{server.server_port}/v1', initialized)
                yield ui, server
            finally:
                if ui:
                    ui.close()
                server.shutdown()
                server.server_close()
                thread.join(timeout=2)

    def test_startup_1_fresh_database_completes_a_turn(self):
        """Given no DB, When a turn runs, Then the actual reply reaches the user."""
        with self.session(initialized=False) as (ui, server):
            ui.turn('Review inverter telemetry')
            self.assertTrue(server.requests)
            self.assertEqual(ui.proc.poll(), None, 'CLI crashed during its first turn')

    def test_startup_2_existing_database_completes_a_turn(self):
        """Given a canonical DB, When a turn runs, Then session references are valid."""
        with self.session() as (ui, server):
            ui.turn('Review inverter telemetry')
            ui.turn('Review battery telemetry')
            self.assertTrue(server.requests)
            self.assertNotIn('FOREIGN KEY constraint failed', ui.pump())
            with sqlite3.connect(ui.home / '.config' / 'nehanda' / 'ona-session.db') as db:
                self.assertEqual(db.execute('SELECT COUNT(*) FROM sessions').fetchone()[0], 1)

    def test_startup_3_clear_registers_a_new_session(self):
        """Given a completed turn, When /clear and another turn run, Then both succeed."""
        with self.session(initialized=False) as (ui, server):
            ui.turn('Review inverter telemetry')
            count = len(server.requests)
            ui.send('/clear\r')
            ui.wait_for('Conversation cleared.')
            ui.output = ''
            ui.turn('Review battery telemetry')
            self.assertGreater(len(server.requests), count)
            with sqlite3.connect(ui.home / '.config' / 'nehanda' / 'ona-session.db') as db:
                self.assertEqual(db.execute('SELECT COUNT(*) FROM sessions').fetchone()[0], 2)
                self.assertEqual(db.execute('SELECT COUNT(*) FROM conversations').fetchone()[0], 2)

    def test_auth_1_next_turn_uses_compatible_key(self):
        """Given two keys, When a compatible turn runs, Then only its key is sent."""
        with self.session() as (ui, server):
            ui.turn('Review inverter telemetry')
            self.assertTrue(server.requests)
            self.assertEqual(server.requests[0][0], 'Bearer dummy-openrouter-key')

    def test_auth_5_wizard_provider_survives_restart(self):
        """Given setup, When configuring and restarting, Then provider/key remain correct."""
        with self.session() as (ui, server):
            config_path = ui.home / '.config' / 'nehanda' / 'agents.json'
            original = json.loads(config_path.read_text())
            original['agents'][0]['provider'] = 'nehanda'
            config_path.write_text(json.dumps(original))
            ui.send('/key openrouter dummy-openrouter-key\r')
            ui.wait_for('API host')
            ui.send(ui.endpoint + '\r')
            ui.wait_for('Select model')
            ui.send('1')
            ui.wait_for('Provider →')
            agents = json.loads((ui.home / '.config' / 'nehanda' / 'agents.json').read_text())
            self.assertEqual(agents['agents'][0].get('provider'), 'openai_compatible')
            ui.turn('Review inverter telemetry')
            self.assertEqual(server.requests[-1][0], 'Bearer dummy-openrouter-key')
            server.requests.clear()
            ui.restart()
            ui.turn('Review battery telemetry')
            self.assertEqual(server.requests[-1][0], 'Bearer dummy-openrouter-key')

    def test_secret_1_typing_is_masked(self):
        """Given a key prompt, When typing, Then no raw key reaches the terminal."""
        with self.session() as (ui, _server):
            ui.send('/key openrouter\r')
            ui.wait_for('New openrouter API key:')
            ui.send('sk-dummy-screen-secret-25')
            self.assertNotIn('sk-dummy-screen-secret-25', ui.pump(0.4))
            ui.send('\r')
            ui.wait_for('API host')
            saved = json.loads((ui.home / 'ona' / 'secure' / 'anthropic.json').read_text())
            self.assertEqual(saved['openaiCompatApiKey'], 'sk-dummy-screen-secret-25')

    def test_secret_2_inline_key_echo_is_redacted(self):
        """Given an inline key, When submitted, Then command echo hides the value."""
        with self.session() as (ui, _server):
            # Type separately from submit so input-editing pixels are not confused
            # with the finalized command echo being tested.
            ui.send('/key openrouter sk-dummy-inline-secret-25')
            ui.pump(0.3)
            ui.output = ''
            ui.send('\r')
            ui.wait_for('API host')
            self.assertNotIn('sk-dummy-inline-secret-25', ui.pump())
            saved = json.loads((ui.home / 'ona' / 'secure' / 'anthropic.json').read_text())
            self.assertEqual(saved['openaiCompatApiKey'], 'sk-dummy-inline-secret-25')

    def test_secret_3_inline_key_typing_is_masked(self):
        """Given an inline key command, When typing its value, Then it is hidden."""
        with self.session() as (ui, _server):
            ui.send('/key openrouter sk-dummy-typed-inline-25')
            self.assertNotIn('sk-dummy-typed-inline-25', ui.pump(0.4))

    def test_secret_4_mcp_assignment_echo_is_redacted(self):
        """Given an MCP env assignment, When submitted, Then the echo hides its value."""
        with self.session() as (ui, _server):
            ui.send('/mcp env no-such-server ENERGY_API_KEY sk-dummy-mcp-25')
            ui.pump()
            ui.output = ''
            ui.send('\r')
            self.assertNotIn('sk-dummy-mcp-25', ui.pump(0.5))

    def test_spin_1_reasoning_progress_reaches_terminal(self):
        """Given reasoning SSE, When chunks arrive, Then progress is rendered."""
        with self.session('reasoning') as (ui, _server):
            ui.turn('Review inverter telemetry')
            output = ui.pump()
            self.assertRegex(output, r'Thinking \(\d+ tokens\)')
            self.assertNotIn('Compare inverter production', output)

    def test_spin_2_cancel_restores_idle_without_stale_progress(self):
        """Given reasoning in flight, When Esc is pressed, Then idle input is restored."""
        with self.session('reasoning') as (ui, _server):
            ui.send('Review inverter telemetry\r')
            ui.wait_for('Use SDLC workflow')
            ui.send('n\r')
            ui.wait_for('Thinking (')
            ui.send('\x1b')
            ui.wait_for('(cancelled)')
            ui.pump(0.3)
            ui.output = ''
            ui.send('/help\r')
            ui.wait_for('Commands:')
            output = ui.pump()
            self.assertNotIn('Cancelling', output)
            self.assertNotIn('Thinking (', output)

    def test_table_1_inverter_rows_render_as_a_bounded_table(self):
        """Given inverter rows, When the tool returns, Then a capped table appears."""
        with self.session('table') as (ui, _server):
            ui.send('Inspect the inverter readings\r')
            ui.wait_for('Use SDLC workflow')
            ui.send('n\r')
            ui.wait_for('Allow Bash?')
            ui.send('y\r')
            ui.wait_for('Telemetry review finished.')
            output = ui.pump()
            self.assertIn('12 rows', output)
            self.assertIn('2 more rows', output)
            self.assertIn('power_kw', output)
            self.assertIn('inverter-9', output)

    def test_input_1_tab_completes_help(self):
        """Given /hel, When Tab and Enter are pressed, Then help executes."""
        with self.session() as (ui, server):
            ui.send('/hel')
            ui.pump()
            ui.send('\t')
            ui.pump()
            ui.send('\r')
            ui.wait_for('Commands:')
            self.assertNotIn('Unknown command:', ui.pump())
            self.assertFalse(server.requests)

    def test_input_2_up_recalls_previous_prompt(self):
        """Given a submitted prompt, When Up/Enter run, Then that prompt is resent."""
        with self.session() as (ui, server):
            ui.turn('Review inverter telemetry')
            ui.pump(0.3)
            count = len(server.requests)
            ui.output = ''
            ui.send('\x1b[A')
            ui.pump()
            ui.send('\r')
            ui.wait_for('Use SDLC workflow', seconds=3)
            ui.send('n\r')
            deadline = time.monotonic() + 3
            while len(server.requests) == count and time.monotonic() < deadline:
                ui.pump()
            self.assertGreater(len(server.requests), count, 'Up/Enter did not resend the prompt')

    def test_input_3_arrows_choose_a_slash_command(self):
        """Given slash suggestions, When Down/Tab/Enter run, Then selected config runs."""
        with self.session() as (ui, server):
            ui.send('/')
            ui.pump()
            for _ in range(3):
                ui.send('\x1b[B')
                ui.pump()
            ui.send('\t')
            ui.pump()
            ui.send('\r')
            ui.wait_for('Provider:')
            self.assertFalse(server.requests)

    def test_input_4_down_restores_the_unsent_draft(self):
        """Given history and a draft, When Up then Down run, Then the draft returns."""
        with self.session() as (ui, server):
            ui.turn('Review inverter telemetry')
            ui.send('Review battery draft')
            ui.pump()
            ui.output = ''
            ui.send('\x1b[A')
            ui.wait_for('Review inverter telemetry', seconds=3)
            ui.output = ''
            ui.send('\x1b[B')
            ui.wait_for('Review battery draft', seconds=3)
            ui.send('\r')
            ui.wait_for('Use SDLC workflow')
            ui.send('n\r')
            ui.wait_for('Telemetry review finished.')
            self.assertIn('Review battery draft', json.dumps(server.requests[-1][1]))

    def test_input_5_secret_commands_are_not_recalled(self):
        """Given a secret assignment after help, When Up/Enter run, Then help is recalled."""
        with self.session() as (ui, server):
            ui.send('/help\r')
            ui.wait_for('Commands:')
            ui.send('/mcp env no-such-server ENERGY_KEY sk-dummy-history-25\r')
            ui.pump(0.3)
            ui.output = ''
            ui.send('\x1b[A')
            ui.pump()
            ui.send('\r')
            ui.wait_for('Commands:', seconds=3)
            self.assertNotIn('sk-dummy-history-25', ui.pump())
            self.assertFalse(server.requests)

    def test_input_6_escape_dismisses_completion_without_changing_input(self):
        """Given a partial slash command, When Esc/Tab/Enter run, Then text is unchanged."""
        with self.session() as (ui, _server):
            ui.send('/hel')
            ui.pump()
            ui.send('\x1b')
            ui.pump()
            ui.send('\t')
            ui.pump()
            ui.send('\r')
            ui.wait_for('Unknown command: /hel')

    def test_input_7_no_match_tab_and_empty_history_are_safe(self):
        """Given no history and an unknown command, When arrows/Tab run, Then text survives."""
        with self.session() as (ui, _server):
            ui.send('\x1b[A')
            ui.pump()
            ui.send('\x1b[B')
            ui.pump()
            ui.send('/no-such-command-25')
            ui.pump()
            ui.send('\t')
            ui.pump()
            ui.send('\r')
            ui.wait_for('Unknown command: /no-such-command-25')

    def test_input_8_enter_completes_selection_before_submission(self):
        """Given a selected suggestion, When Enter runs twice, Then it completes then submits."""
        with self.session() as (ui, server):
            ui.send('/hel')
            ui.pump()
            ui.send('\x1b[B')
            ui.pump()
            ui.output = ''
            ui.send('\r')
            first = ui.pump(0.3)
            self.assertNotIn('Unknown command:', first)
            self.assertNotIn('Commands:', first)
            ui.send('\r')
            ui.wait_for('Commands:')
            self.assertFalse(server.requests)

    def test_input_9_consecutive_duplicates_do_not_hide_previous_commands(self):
        """Given repeated help after MCP help, When Up twice runs, Then MCP help is recalled."""
        with self.session() as (ui, _server):
            ui.send('/mcp\r')
            ui.wait_for('MCP Server & Tool Commands:')
            for _ in range(2):
                ui.output = ''
                ui.send('/help\r')
                ui.wait_for('Commands:')
            ui.output = ''
            for _ in range(2):
                ui.send('\x1b[A')
                ui.pump()
            ui.send('\r')
            ui.wait_for('MCP Server & Tool Commands:', seconds=3)

    def test_input_10_history_is_bounded_to_the_newest_100_entries(self):
        """Given 101 commands, When recall reaches its lower bound, Then entry one remains."""
        with self.session() as (ui, _server):
            for i in range(101):
                ui.output = ''
                ui.send(f'/history-entry-{i}\r')
                ui.wait_for(f'Unknown command: /history-entry-{i}')
            ui.output = ''
            for _ in range(105):
                ui.send('\x1b[A')
                ui.pump(0.02)
            ui.send('\r')
            output = ui.pump(0.5)
            submitted = re.findall(r'(?:^|\n)❯ (/history-entry-\d+)\r?\n', output)
            self.assertTrue(submitted, 'Expected a finalized user command after recall')
            self.assertEqual(submitted[-1], '/history-entry-1')

    def test_input_11_completion_does_not_insert_argument_placeholders(self):
        """Given a command requiring arguments, When Tab completes, Then only literal words remain."""
        with self.session() as (ui, _server):
            ui.send('/mcp a')
            ui.pump()
            ui.output = ''
            ui.send('\t')
            output = ui.pump(0.3)
            self.assertIn('/mcp add', output)
            self.assertNotIn('<name>', output)
            self.assertNotIn('<command>', output)
            ui.send(f'energy-fixture {NODE} --version\r')
            ui.wait_for('Added energy-fixture')
            config = json.loads((ui.home / '.config' / 'nehanda' / 'mcp.json').read_text())
            self.assertEqual(config['mcpServers']['energy-fixture']['command'], NODE)
            self.assertEqual(config['mcpServers']['energy-fixture']['args'], ['--version'])

    def test_sandbox_1_menu_executes_in_a_disposable_copy(self):
        """Given a blocked tool, When the user chooses S, Then real output and original files remain."""
        with self.session('sandbox') as (ui, _server):
            original = ui.home / 'telemetry.csv'
            original.write_text('original\n')
            ui.send('Run the isolated telemetry check\r')
            ui.wait_for('Use SDLC workflow')
            ui.send('n\r')
            ui.wait_for('Allow Bash?')
            ui.send('y\r')
            ui.wait_for('Command Blocked')
            ui.send('s')
            ui.wait_for('Telemetry review finished.', seconds=20)
            self.assertRegex(ui.pump(), r'└ ✓ [^\r\n]*sandbox-cli-executed')
            self.assertEqual(original.read_text(), 'original\n')

    def test_sandbox_2_escape_removes_the_running_container(self):
        """Given a running sandbox command, When Esc is pressed, Then it cancels and is removed."""
        with self.session('sandbox_slow') as (ui, _server):
            ui.send('Run the isolated telemetry check\r')
            ui.wait_for('Use SDLC workflow')
            ui.send('n\r')
            ui.wait_for('Allow Bash?')
            ui.send('y\r')
            ui.wait_for('Command Blocked')
            ui.send('s')
            with sqlite3.connect(ui.home / '.config' / 'nehanda' / 'ona-session.db') as db:
                session_id = db.execute('SELECT session_id FROM sessions ORDER BY started_at DESC LIMIT 1').fetchone()[0]
            args = ['docker', 'ps', '-q', '--filter', 'label=nehanda.sandbox.session=' + session_id]
            deadline = time.monotonic() + 10
            running = False
            while time.monotonic() < deadline:
                result = subprocess.run(args, capture_output=True, text=True, timeout=5, check=True)
                if result.stdout.strip():
                    running = True
                    break
                ui.pump(0.1)
            self.assertTrue(running, 'The chosen sandbox never ran a container')
            ui.send('\x1b')
            ui.wait_for('(cancelled)', seconds=12)
            result = subprocess.run(args, capture_output=True, text=True, timeout=5, check=True)
            self.assertEqual(result.stdout.strip(), '')


if __name__ == '__main__':
    if '--without-sandbox' in sys.argv:
        suite = unittest.defaultTestLoader.loadTestsFromTestCase(TuiUsabilityBehavioral)
        core = unittest.TestSuite(test for test in suite if not test.id().split('.')[-1].startswith('test_sandbox_'))
        result = unittest.TextTestRunner(verbosity=2).run(core)
        sys.exit(0 if result.wasSuccessful() else 1)
    unittest.main(verbosity=2)
