#!/usr/bin/env python3
"""Transparent bidirectional PTY transport for real Unix terminal integration tests.

Usage: python3 tests/fixtures/pty_driver.py COMMAND [ARGS...]

Only the external terminal transport is simulated. The command itself runs in
its own controlling pseudo-terminal with real TTY stdin/stdout/stderr.
Uses Python's standard library and supports macOS and Linux.
"""
import errno
import fcntl
import os
import pty
import select
import signal
import struct
import sys
import termios


def write_all(fd, data):
    """Never truncate escape sequences or terminal frames on short pipe writes."""
    offset = 0
    while offset < len(data):
        try:
            written = os.write(fd, data[offset:])
        except InterruptedError:
            continue
        if written <= 0:
            raise OSError('PTY transport closed during write')
        offset += written


def main():
    if len(sys.argv) < 2:
        print('Usage: pty_driver.py COMMAND [ARGS...]', file=sys.stderr)
        return 2
    pid, master = pty.fork()
    if pid == 0:
        try:
            os.execvpe(sys.argv[1], sys.argv[1:], os.environ)
        except OSError as exc:
            print('PTY child exec failed: %s' % exc, file=sys.stderr)
            os._exit(127)

    # A process launched under Ink must have a real, dimensioned TTY.
    rows = int(os.environ.get('LINES', '40'))
    cols = int(os.environ.get('COLUMNS', '120'))
    fcntl.ioctl(master, termios.TIOCSWINSZ, struct.pack('HHHH', rows, cols, 0, 0))

    # Ink 6.8.0 distinguishes CR (Return) from LF (Enter) and ink-text-input
    # submits only on Return. A newly allocated pseudo-terminal can have ICRNL
    # enabled even after Node sets raw mode; forwarding b'\r' would then deliver
    # b'\n', displaying the typed prompt but never invoking onSubmit. Preserve
    # the bytes produced by the synthetic terminal keyboard instead.
    attrs = termios.tcgetattr(master)
    attrs[0] &= ~(termios.ICRNL | termios.INLCR)
    termios.tcsetattr(master, termios.TCSANOW, attrs)

    def terminate(_signum, _frame):
        try:
            os.kill(pid, signal.SIGTERM)
        except ProcessLookupError:
            pass

    signal.signal(signal.SIGTERM, terminate)
    signal.signal(signal.SIGINT, terminate)
    stdin_open = True
    try:
        while True:
            readfds = [master]
            if stdin_open:
                readfds.append(sys.stdin.fileno())
            ready, _, _ = select.select(readfds, [], [], 0.15)
            if sys.stdin.fileno() in ready:
                data = os.read(sys.stdin.fileno(), 4096)
                if not data:
                    stdin_open = False
                else:
                    write_all(master, data)
            if master in ready:
                try:
                    out = os.read(master, 8192)
                except OSError as exc:
                    if exc.errno != errno.EIO:
                        raise
                    break
                if not out:
                    break
                write_all(sys.stdout.fileno(), out)
    finally:
        os.close(master)
    _, status = os.waitpid(pid, 0)
    return os.waitstatus_to_exitcode(status)


if __name__ == '__main__':
    sys.exit(main())
