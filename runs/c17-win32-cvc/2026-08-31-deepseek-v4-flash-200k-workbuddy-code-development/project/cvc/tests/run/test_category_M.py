# Category M - Rollback.
from testlib import *
import os

s = suite("M-Rollback")


def _setup(r):
    r.init()
    r.write("a.txt", "A1\n")
    r.run("save", "-m", "c1")
    c1 = saved_commit_id(r)
    r.write("a.txt", "A2\n")
    r.write("b.txt", "B\n")
    r.run("save", "-m", "c2")
    c2 = saved_commit_id(r)
    return c1, c2


# M01: rollback ancestor tree
def case_m01():
    r = new_repo("m01"); c1, c2 = _setup(r)
    p = r.run("rollback", c1, "-m", "back to c1")
    assert p.returncode == 0, p.stderr.decode()
    assert r.read("a.txt") == b"A1\n", "a.txt must be ancestor content"
    assert not r.exists("b.txt"), "b.txt (from later commit) must be gone"
s.test("M01 rollback ancestor tree", case_m01)


# M02: rollback cross-branch commit tree
def case_m02():
    r = new_repo("m02"); _setup(r)
    r.run("branch", "create", "feat")
    r.run("switch", "feat")
    r.write("feat.txt", "F\n")
    r.run("save", "-m", "feat")
    fcid = saved_commit_id(r)
    r.run("switch", "main")
    p = r.run("rollback", fcid, "-m", "back to feat")
    assert p.returncode == 0, p.stderr.decode()
    assert r.read("feat.txt") == b"F\n", "must materialize the other branch's tree"
s.test("M02 rollback cross-branch commit tree", case_m02)


# M03: rollback creates a new commit (history grows)
def case_m03():
    r = new_repo("m03"); c1, c2 = _setup(r)
    before = saved_commit_id(r)  # c2
    p = r.run("rollback", c1, "-m", "back")
    assert p.returncode == 0, p.stderr.decode()
    after = saved_commit_id(r)
    assert after != before, "rollback must create a new commit"
    assert after != c1, "rollback commit is a new object, not the target"
s.test("M03 rollback creates new commit", case_m03)


# M04: rollback parent is pre-rollback HEAD, not target
def case_m04():
    r = new_repo("m04"); c1, c2 = _setup(r)
    p = r.run("rollback", c1, "-m", "back")
    assert p.returncode == 0
    # The rollback commit's single parent is the pre-rollback HEAD (c2), so c2
    # must appear in the first-parent history immediately before the rollback.
    log = r.run("log").stdout.decode("utf-8")
    assert c2 in log, "pre-rollback HEAD (c2) must be reachable"
    assert c1 in log, "target commit must also be reachable via c2's parent chain"
s.test("M04 rollback parent is pre-rollback HEAD", case_m04)


# M05: old history remains reachable through parents
def case_m05():
    r = new_repo("m05"); c1, c2 = _setup(r)
    r.run("rollback", c1, "-m", "back")
    log = r.run("log").stdout.decode("utf-8")
    assert c1 in log and c2 in log, "both old commits must remain in history"
s.test("M05 old history remains reachable", case_m05)


# M06: dirty working tree blocks rollback
def case_m06():
    r = new_repo("m06"); c1, c2 = _setup(r)
    r.write("a.txt", "DIRTY\n")
    p = r.run("rollback", c1, "-m", "back")
    assert p.returncode != 0, "dirty tree must block rollback"
    assert saved_commit_id(r) == c2, "ref must not move on blocked rollback"
s.test("M06 dirty tree blocks rollback", case_m06)


# M07: collision blocks rollback without ref movement
def case_m07():
    r = new_repo("m07"); c1, c2 = _setup(r)
    # untracked dir where rollback target wants a file (a.txt present in c1)
    r.unlink("a.txt")
    os.makedirs(os.path.join(r.path, "a.txt"))  # dir blocks file
    p = r.run("rollback", c1, "-m", "back")
    assert p.returncode != 0, "collision must block rollback"
    assert saved_commit_id(r) == c2, "ref must not move"
s.test("M07 collision blocks rollback", case_m07)


# M08: explicit rollback to equal tree still creates a new single-parent commit
def case_m08():
    r = new_repo("m08"); c1, c2 = _setup(r)
    before = saved_commit_id(r)
    p = r.run("rollback", c2, "-m", "no-op rollback")
    assert p.returncode == 0, p.stderr.decode()
    after = saved_commit_id(r)
    assert after != before, "explicit rollback to equal tree must still create a commit"
    log = r.run("log").stdout.decode("utf-8")
    assert "no-op rollback" in log
s.test("M08 rollback to equal tree creates commit", case_m08)


# M09: rollback on unborn current branch fails without creating history
def case_m09():
    r = new_repo("m09")
    r.init()
    p = r.run("rollback", "main", "-m", "back")
    assert p.returncode != 0, "rollback on unborn branch must fail"
    # no objects created
    objdir = os.path.join(r.path, ".cvc", "objects")
    n = 0
    if os.path.isdir(objdir):
        for _, _, files in os.walk(objdir):
            n += len(files)
    assert n == 0, "no objects must be created by failed rollback"
s.test("M09 rollback on unborn branch fails", case_m09)
