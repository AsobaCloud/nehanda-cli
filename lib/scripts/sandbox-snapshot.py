"""Issue #25: copy a bounded project tree without following symlinks."""

import os
from pathlib import Path
import stat
import sys


MAX_BYTES = 256 * 1024 * 1024
MAX_ENTRIES = 10000
MAX_DEPTH = 64
EXCLUDED = {
    '.git', '.ssh', '.aws', '.azure', '.config', '.ona', '.nehanda',
    '.gnupg', '.kube', '.docker', '.npmrc', '.pypirc', '.netrc', '.env',
    'node_modules', '.venv', 'venv', '.cache', '.pytest_cache', '__pycache__',
}


def copy_project(source, destination):
    """Anchor traversal to directory descriptors, including during renames."""
    total = 0
    entries = 0

    def visit(directory_fd, target, depth):
        nonlocal total, entries
        if depth > MAX_DEPTH:
            raise ValueError('snapshot depth limit exceeded')
        for name in os.listdir(directory_fd):
            lower = name.lower()
            if lower in EXCLUDED or lower.startswith('.env.') or lower.endswith(('.pem', '.key')):
                continue
            entry = os.stat(name, dir_fd=directory_fd, follow_symlinks=False)
            if not (stat.S_ISDIR(entry.st_mode) or stat.S_ISREG(entry.st_mode)):
                continue
            entries += 1
            if entries > MAX_ENTRIES:
                raise ValueError('snapshot entry limit exceeded (10000 entries)')
            flags = os.O_RDONLY | os.O_NOFOLLOW | os.O_NONBLOCK
            if stat.S_ISDIR(entry.st_mode):
                flags |= os.O_DIRECTORY
            fd = os.open(name, flags, dir_fd=directory_fd)
            try:
                opened = os.fstat(fd)
                if (opened.st_dev, opened.st_ino, stat.S_IFMT(opened.st_mode)) != (
                    entry.st_dev, entry.st_ino, stat.S_IFMT(entry.st_mode)
                ):
                    raise ValueError('snapshot source changed during copy')
                dest = target / name
                if stat.S_ISDIR(opened.st_mode):
                    dest.mkdir(mode=0o755)
                    dest.chmod(0o755)
                    visit(fd, dest, depth + 1)
                else:
                    if total + opened.st_size > MAX_BYTES:
                        raise ValueError('snapshot size limit exceeded (256 MiB)')
                    with dest.open('xb') as output:
                        while True:
                            data = os.read(fd, 65536)
                            if not data:
                                break
                            total += len(data)
                            if total > MAX_BYTES:
                                raise ValueError('snapshot size limit exceeded (256 MiB)')
                            output.write(data)
                    dest.chmod(0o555 if opened.st_mode & 0o111 else 0o444)
            finally:
                os.close(fd)

    source_fd = os.open(os.path.realpath(source), os.O_RDONLY | os.O_DIRECTORY | os.O_NOFOLLOW)
    try:
        target = Path(destination)
        target.mkdir(mode=0o755)
        target.chmod(0o755)
        visit(source_fd, target, 0)
    finally:
        os.close(source_fd)


if __name__ == '__main__':
    try:
        copy_project(sys.argv[1], sys.argv[2])
    except (OSError, ValueError) as error:
        print('Project snapshot failed: ' + str(error), file=sys.stderr)
        sys.exit(1)
