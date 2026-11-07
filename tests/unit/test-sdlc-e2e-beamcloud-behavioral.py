#!/usr/bin/env python3
"""Issue #34: real end-to-end SDLC behavior using real ollama/beam:cloud.

This exercises the actual Nehanda TUI and the real ollama beam:cloud model.
Only the Laya sidecar is simulated so that the test remains deterministic about
startup cost; the model itself is a real external dependency, as requested.
"""

import fcntl
import json
import os
import pty
import re
import select
import subprocess
import sys
import tempfile
import time
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
NODE = str(Path(__file__).resolve().parents[2] / 'node_modules' / '.bin' / 'node') if False else __import__('shutil').which('node')
ANSI = re.compile(r'\x1b\[[0-9;?]*[A-Za-z]')


def clean(text):
    return ANSI.sub('', text).replace('\r', '\n')


def main():
    home = Path(tempfile.mkdtemp(prefix='nehanda-beamcloud-34-'))
    config = home / '.config' / 'nehanda'
    config.mkdir(parents=True, exist_ok=True)

    # Create the same schema lazily through the store initializer so that the
    # first persisted session is valid if a turn completes.
    module = (ROOT / 'lib' / 'store.mjs').as_uri()
    initializer = (
        "import {openStore,closeStore} from " + json.dumps(module) + ";"
        "const p=" + json.dumps(str(config / 'ona-session.db')) + ";"
        "openStore(p);closeStore(p);"
    )
    subprocess.run([NODE, '--input-type=module', '-e', initializer],
                   check=True, capture_output=True, text=True)

    (config / '.splash-seen').touch()
    (config / 'agents.json').write_text(json.dumps({
        'default_agent': 'test',
        'agents': [{
            'name': 'test',
            'provider': 'ollama',
            'model': 'beam:cloud',
            'endpoint': 'http://localhost:11434/v1',
            'api_key': 'ollama',
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
        '    try: msg = json.loads(line)\n'
        '    except Exception:\n'
        '        print(json.dumps({"id":None,"ok":True}),flush=True); continue\n'
        '    result = {"loaded": True} if msg.get("method") == "load" else {}\n'
        '    print(json.dumps({"id": msg.get("id"), "ok": True, "result": result}), flush=True)\n'
    )
    worker.chmod(0o700)

    # Create a tiny project directory containing MAP.md so the explorer can at
    # least complete its first registry read if the model follows the contract.
    project = home / 'project'
    project.mkdir()
    (project / 'MAP.md').write_text(
        "# MAP\nrouter: src/index.mjs\nfiles:\n  - src/health-check.mjs\n", 'utf8'
    )
    (project / 'src').mkdir()
    (project / 'src' / 'health-check.mjs').write_text(
        "export function healthCheck() { return { ok: true, status: 200 } }\n", 'utf8')

    env = {k: v for k, v in os.environ.items() if k not in {
        'OPENAI_API_KEY', 'OPENROUTER_API_KEY', 'NEHANDA_API_KEY', 'ZAI_API_KEY',
    }}
    env.update({
        'HOME': str(home),
        'ONAHOME': str(home / 'ona'),
        'NEHANDA_HOME': str(laya),
        'LAYA_PYTHON': str(worker),
        'OPENROUTER_API_KEY': 'dummy-openrouter-key',
        'SDLC_DISABLE_ALL_HOOKS': '1',
        'TERM': 'xterm-256color',
    })

    master, slave = pty.openpty()
    proc = subprocess.Popen(
        [NODE, str(ROOT / 'bin' / 'nehanda-ui.mjs')],
        cwd=project, env=env, stdin=slave, stdout=slave, stderr=slave,
        start_new_session=True,
    )
    os.close(slave)
    fcntl.fcntl(master, fcntl.F_SETFL, os.O_NONBLOCK)

    output = ''

    def pump(seconds=0.25):
        nonlocal output
        deadline = time.monotonic() + seconds
        while time.monotonic() < deadline:
            ready, _, _ = select.select([master], [], [], 0.05)
            if not ready:
                continue
            try:
                raw = os.read(master, 65536)
            except (OSError, BlockingIOError):
                break
            if not raw:
                break
            output += raw.decode(errors='replace')
        return ANSI.sub('', output)

    def send(text, delay=0.15):
        os.write(master, text.encode())
        pump(delay)

    def wait_for(text, timeout=120):
        nonlocal output
        deadline = time.monotonic() + timeout
        while time.monotonic() < deadline:
            if text in pump(0.5):
                return True
        return False

    try:
        assert wait_for('Type /help for commands', 30), \
            'CLI failed to start: ' + clean(output)[-2000:]

        # Start a real explore prompt. Accept the SDLC workflow so that the
        # explore gate, tool loop, and plan gate actually run.
        question = "I'd like to review the health check service on asobacorp-1:3000 and make sure it reflects current state"
        os.write(master, (question + '\r').encode())
        pump(0.5)
        showed = wait_for('Use SDLC workflow', 60)
        if not showed:
            raise AssertionError('Never got SDLC prompt: ' + clean(output)[-4000:])
        time.sleep(0.25)
        pump(0.1)
        os.write(master, b'y\r')
        pump(0.5)

        # The model may take noticeable time on a remote 501 B model.
        reached_exploration_complete = wait_for('Exploration complete.', 180)
        clean_text = clean(output)
        print('\n--- E2E BEAMCLOUD OUTPUT (tail) ---')
        print(clean_text[-8000:])
        print('--- END OUTPUT ---\n')

        if not reached_exploration_complete:
            print('[FAIL] Never reached "Exploration complete." within timeout.', file=sys.stderr)

        clean_text = clean(output)
        duplicate_guard_hits = clean_text.count('was already read in this exploration')
        # (canonical duplicate guard marker count checked via duplicate_guard_hits)
        read_map_hits = clean_text.count('Read(MAP.md')

        print(f'metrics: read_map_mentions={read_map_hits} duplicate_guard={duplicate_guard_hits} exploration_complete={reached_exploration_complete}')

        # Capture an artifact outside the repo root, as required by AGENTS
        # Commandment 8.
        artifact = Path('/tmp/beamcloud-e2e-output.txt')
        artifact.write_text(clean_text, encoding='utf-8')
        print(f'artifact: {artifact}')

        assert reached_exploration_complete, 'Exploration never converged to "Exploration complete."'
        assert duplicate_guard_hits <= 1, (
            'Tool complained about a duplicate MAP.md read '
            + str(duplicate_guard_hits) + ' times; output tail: '
            + repr(clean_text[-3000:]))

        # The criterion "would this test fail if wrong outcome?" — if the loop
        # repeats, at least one of read_map_mentions or duplicate_guard_hits
        # rises, so the assertions above fail; ping checks are intentionally not used.
        assert read_map_hits <= 2, f'Read(MAP.md) appeared {read_map_hits} times'
    finally:
        proc.terminate()
        try:
            proc.wait(timeout=5)
        except subprocess.TimeoutExpired:
            proc.kill()
            proc.wait()
        os.close(master)
        # Do not delete home automatically; preserve for diagnosis. /tmp allowed.
        print(f'work Home preserved at {home}')


if __name__ == '__main__':
    main()
