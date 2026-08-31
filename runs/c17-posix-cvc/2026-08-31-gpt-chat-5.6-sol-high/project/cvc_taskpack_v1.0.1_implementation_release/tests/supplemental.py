#!/usr/bin/env python3
import os
import shutil
import subprocess
import sys
import tempfile
from pathlib import Path

BIN = Path(sys.argv[1] if len(sys.argv) > 1 else './cvc').resolve()
ROOT = BIN.parent
FAULT = ROOT / 'tests/libfaultio.so'

def run(args, cwd, ok=True, env=None):
    e = os.environ.copy()
    if env:
        e.update({k: str(v) for k, v in env.items()})
    p = subprocess.run([str(BIN)] + list(args), cwd=cwd, env=e,
                       stdout=subprocess.PIPE, stderr=subprocess.PIPE, timeout=30)
    if (p.returncode == 0) != ok:
        raise AssertionError(f'{args}: rc={p.returncode}\nstdout={p.stdout!r}\nstderr={p.stderr!r}')
    return p

def repo():
    r = tempfile.mkdtemp(prefix='cvc-supp-')
    run(['init'], r)
    return r

def test_restore_structural_symlink_no_follow():
    r = repo()
    outside = tempfile.mkdtemp(prefix='cvc-supp-outside-')
    try:
        (Path(r) / 'd').mkdir()
        (Path(r) / 'd/a').write_text('A', encoding='utf-8')
        run(['save', '-m', 'base'], r, env={'CVC_TEST_TIMESTAMP': 1})
        shutil.rmtree(Path(r) / 'd')
        os.symlink(outside, Path(r) / 'd')
        run(['restore', 'd', '--from', 'main'], r)
        assert (Path(r) / 'd').is_dir() and not (Path(r) / 'd').is_symlink()
        assert (Path(r) / 'd/a').read_text(encoding='utf-8') == 'A'
        assert list(Path(outside).iterdir()) == []
    finally:
        shutil.rmtree(r, ignore_errors=True)
        shutil.rmtree(outside, ignore_errors=True)

def test_post_rename_fsync_failure_rolls_back_new_leaf():
    r = repo()
    try:
        run(['branch', 'create', 'dev'], r)
        run(['switch', 'dev'], r)
        (Path(r) / 'new.txt').write_text('target', encoding='utf-8')
        run(['save', '-m', 'dev'], r, env={'CVC_TEST_TIMESTAMP': 1})
        run(['switch', 'main'], r)
        assert not (Path(r) / 'new.txt').exists()
        run(['switch', 'dev'], r, ok=False,
            env={'LD_PRELOAD': FAULT, 'FI_MODE': 'work-fsync', 'FI_MATCH': 'new.txt'})
        assert not (Path(r) / 'new.txt').exists()
        assert (Path(r) / '.cvc/HEAD').read_text(encoding='utf-8') == 'ref: refs/heads/main\n'
    finally:
        shutil.rmtree(r, ignore_errors=True)

tests = [
    ('restore structural symlink without follow', test_restore_structural_symlink_no_follow),
    ('post-rename fsync failure rollback', test_post_rename_fsync_failure_rolls_back_new_leaf),
]
for name, fn in tests:
    fn()
    print(f'PASS supplemental: {name}')
print(f'SUPPLEMENTAL SUMMARY: {len(tests)}/{len(tests)} passed')
