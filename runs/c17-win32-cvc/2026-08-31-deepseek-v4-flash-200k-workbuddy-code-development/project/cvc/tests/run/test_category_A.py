# Category A - Build and Smoke
from testlib import *

s = suite("A-BuildAndSmoke")

# A02: --help exits 0 and lists required commands
def case_a02():
    r = new_repo("a02")
    p = r.run("--help")
    assert p.returncode == 0, "help rc=%d" % p.returncode
    out = p.stdout.decode("utf-8", "replace")
    for cmd in ["init", "status", "save", "log", "diff", "branch", "switch",
                "restore", "rollback", "merge", "resolve", "verify", "config",
                "help"]:
        assert cmd in out, "help missing %s" % cmd
    assert "continue" in out and "abort" in out
s.test("A02 help exits 0 and lists commands", case_a02)

# A03: unknown command fails nonzero
def case_a03():
    r = new_repo("a03")
    p = r.run("bogus")
    assert p.returncode != 0, "unknown cmd should fail"
s.test("A03 unknown command fails nonzero", case_a03)

# A04: missing repository error handled
def case_a04():
    r = new_repo("a04")
    for cmd in [("status",), ("log",), ("save", "-m", "x")]:
        p = r.run(*cmd)
        assert p.returncode != 0, "%s in non-repo should fail" % cmd[0]
        assert b"not a repository" in p.stderr.lower() or b"repository" in p.stderr.lower(), \
            "diagnostic missing for %s" % cmd[0]
s.test("A04 missing repository error handled", case_a04)

# A05: help lists resolve plus merge continue/abort forms
def case_a05():
    r = new_repo("a05")
    p = r.run("--help")
    out = p.stdout.decode()
    assert "resolve" in out
    assert "merge --continue" in out and "merge --abort" in out
s.test("A05 help lists resolve + merge continue/abort", case_a05)

# A06: log --max-count canonical + reject bad forms
def case_a06():
    r = new_repo("a06")
    r.init()
    r.write("f", "a")
    r.run("save", "-m", "one")
    r.run("save", "-m", "two")
    p = r.run("log", "--max-count=1")
    assert p.returncode == 0
    assert p.stdout.decode().count("commit ") == 1, "max-count=1 should show 1"
    for bad in ["--max-count=0", "--max-count=-1", "--max-count=01",
                "--max-count=", "--max-count=abc", "--max-count=18446744073709551616"]:
        p = r.run("log", bad)
        assert p.returncode != 0, "max-count %s should be rejected" % bad
s.test("A06 log --max-count parsing", case_a06)

# A07: nested subdirectory operates on full repo root
def case_a07():
    r = new_repo("a07")
    r.init()
    r.write("dir/a.txt", "root level")
    r.write("sub/x.txt", "sub file")
    r.run("save", "-m", "root")
    # create a change at root, then run status from nested subdir
    r.write("dir/a.txt", "changed at root")
    nested = os.path.join(r.path, "sub")
    p = r.run("status", cwd=nested)
    assert "a.txt" in p.stdout.decode(), "status from subdir should see root change"
    # save from nested subdir records full tree
    r.run("save", "-m", "from nested", cwd=nested)
    p = r.run("log", cwd=nested)
    assert "from nested" in p.stdout.decode()
s.test("A07 nested subdir operates on repo root", case_a07)

# A08: duplicate singleton options rejected
def case_a08():
    r = new_repo("a08")
    r.init()
    r.write("f", "x")
    # duplicate -m
    p = r.run("save", "-m", "a", "-m", "b")
    assert p.returncode != 0, "duplicate -m should be rejected"
    # duplicate --max-count
    r.run("save", "-m", "one")
    p = r.run("log", "--max-count=1", "--max-count=2")
    assert p.returncode != 0, "duplicate --max-count should be rejected"
    # duplicate --include
    p = r.run("status", "--include=a", "--include=b")
    assert p.returncode != 0, "duplicate --include should be rejected"
s.test("A08 duplicate singleton options rejected", case_a08)

# A09: branch/path list deterministic unsigned-byte ordering
def case_a09():
    r = new_repo("a09")
    r.init()
    for name in ["zeta.txt", "Alpha.txt", "b.txt", "中.txt"]:
        r.write(name, "x")
    r.run("save", "-m", "many")
    r.write("new1.txt", "a"); r.write("new2.txt", "b")
    p = r.run("status")
    lines = [ln.split()[-1] for ln in p.stdout.decode().splitlines()]
    # verify ascending unsigned byte order of names
    byts = [ln.encode("utf-8") for ln in lines]
    assert byts == sorted(byts), "status paths not in byte order"
s.test("A09 deterministic byte ordering", case_a09)

# A10: Unicode CLI args round-trip
def case_a10():
    r = new_repo("a10")
    r.init()
    r.write("中文文件.txt", "内容 with emoji \U0001F600")
    p = r.run("save", "-m", "提交消息 with emoji \U0001F600")
    assert p.returncode == 0, p.stderr
    p = r.run("log")
    out = p.stdout.decode("utf-8")
    assert "提交消息 with emoji \U0001F600" in out, "message round-trip failed"
s.test("A10 Unicode CLI args round-trip", case_a10)

# A11: no shell/cygwin dependency for required behavior (implicit - we run cvc directly)
def case_a11():
    # cvc.exe must be a native PE that runs directly without a shell.
    assert os.path.isfile(CVC)
    r = new_repo("a11")
    p = r.run("init")
    assert p.returncode == 0
s.test("A11 native execution without shell", case_a11)

# A12: redirected/piped stdout is UTF-8 independent of console code page
def case_a12():
    r = new_repo("a12")
    r.init()
    r.write("中文.txt", "emoji \U0001F680 content")
    p = r.run("save", "-m", "消息 \U0001F680")
    # capture bytes; ensure valid UTF-8 encoding of the repo-visible text
    p = r.run("log")
    raw = p.stdout
    out = raw.decode("utf-8")  # must not raise
    assert "消息 \U0001F680" in out
    # status path
    r.write("另一个.txt", "z")
    p = r.run("status")
    assert "另一个.txt".encode("utf-8") in p.stdout
s.test("A12 redirected output is UTF-8", case_a12)
