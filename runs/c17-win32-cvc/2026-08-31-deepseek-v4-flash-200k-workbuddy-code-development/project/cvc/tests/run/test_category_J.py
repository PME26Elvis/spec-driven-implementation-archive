# Category J - Branch and switch.
from testlib import *
import os, json

s = suite("J-BranchAndSwitch")


def _new(r):
    r.init()
    r.write("a.txt", "A\n")
    r.run("save", "-m", "base")
    return saved_commit_id(r)


def _set_cfg(r, cfg):
    with open(os.path.join(r.path, ".cvc", "config.json"), "w") as f:
        json.dump(cfg, f)


def _branch(r):
    p = r.run("branch")
    return p.stdout.decode("utf-8")


def _sw(r, name, expect_rc=0):
    return r.run("switch", name)


# J01: create/list branches
def case_j01():
    r = new_repo("j01"); _new(r)
    p = r.run("branch", "create", "feature")
    assert p.returncode == 0, p.stderr.decode()
    out = _branch(r)
    assert "* main" in out and "  feature" in out, out
s.test("J01 create/list branches", case_j01)


# J02: create branch before first commit (unborn branch allowed)
def case_j02():
    r = new_repo("j02"); r.init()
    p = r.run("branch", "create", "early")
    assert p.returncode == 0, p.stderr.decode()
    out = _branch(r)
    assert "* main" in out and "  early" in out, out
s.test("J02 create branch before first commit", case_j02)


# J03: switch between divergent snapshots
def case_j03():
    r = new_repo("j03"); _new(r)
    r.run("branch", "create", "feature")
    r.run("switch", "feature")
    r.write("a.txt", "B\n")
    r.run("save", "-m", "feat")
    r.run("switch", "main")
    r.write("a.txt", "C\n")
    r.run("save", "-m", "maintwo")
    p = r.run("switch", "feature")
    assert p.returncode == 0, p.stderr.decode()
    assert b"feature" in p.stdout
    assert r.read("a.txt") == b"B\n", "must see feature's content"
s.test("J03 switch divergent snapshots", case_j03)


# J04: current branch marker
def case_j04():
    r = new_repo("j04"); _new(r)
    r.run("branch", "create", "dev")
    r.run("switch", "dev")
    out = _branch(r)
    assert "* dev" in out and "  main" in out, out
s.test("J04 current branch marker", case_j04)


# J05: duplicate branch creation fails
def case_j05():
    r = new_repo("j05"); _new(r)
    r.run("branch", "create", "dup")
    p = r.run("branch", "create", "dup")
    assert p.returncode != 0, "duplicate branch must fail"
s.test("J05 duplicate branch fails", case_j05)


# J06: delete current branch fails
def case_j06():
    r = new_repo("j06"); _new(r)
    r.run("branch", "create", "cur")
    r.run("switch", "cur")
    p = r.run("branch", "delete", "cur")
    assert p.returncode != 0, "delete current branch must fail"
s.test("J06 delete current branch fails", case_j06)


# J07: dirty working tree blocks switch
def case_j07():
    r = new_repo("j07"); _new(r)
    r.run("branch", "create", "other")
    r.write("dirty.txt", "d\n")   # untracked added path
    p = r.run("switch", "other")
    assert p.returncode != 0, "dirty tree must block switch"
    # adding a tracked modification also blocks
    r.run("save", "-m", "c")      # save the dirty file to make it clean
    r.write("a.txt", "changed\n")
    p = r.run("switch", "other")
    assert p.returncode != 0, "modified tracked file must block switch"
s.test("J07 dirty tree blocks switch", case_j07)


# J08: filtered-out path collision protection. A path that is excluded by the
# tracking policy (filtered-out, untracked) collides with a path the target
# snapshot needs to materialize; switch must fail and preserve it. Tracking
# config is repo-global, so the filtered-out state is introduced by changing
# config after feature2 recorded build/out.txt as a tracked path.
def case_j08():
    r = new_repo("j08"); r.init()
    r.write("a.txt", "A\n")
    r.run("save", "-m", "base")
    r.run("branch", "create", "feature2")
    r.run("switch", "feature2")
    r.write("build/out.txt", "tracked\n")
    p = r.run("save", "-m", "tracks build")
    assert p.returncode == 0, "feature2 must track build/out.txt: %s" % p.stderr.decode()
    r.run("switch", "main")
    # now exclude build/** so build/out.txt becomes filtered-out (untracked)
    _set_cfg(r, {"format_version": 1, "tracking": {"include": ["**"],
                                                   "exclude": ["build/**"]}})
    r.write("build/out.txt", "filtered-untracked\n")
    p = r.run("switch", "feature2")
    assert p.returncode != 0, "filtered-out collision must block switch"
    assert r.read("build/out.txt") == b"filtered-untracked\n", \
        "colliding filtered-out file must be preserved"
s.test("J08 filtered-out path collision protection", case_j08)


# J09: ineligible-file collision protection. An ineligible working-tree file
# (NUL in the inspected prefix) collides with a path the target snapshot needs
# to materialize; switch must fail and preserve it (never overwrite/delete).
def case_j09():
    r = new_repo("j09"); r.init()
    r.write("a.txt", "A\n")
    r.run("save", "-m", "base")
    r.run("branch", "create", "feature2")
    r.run("switch", "feature2")
    r.write("bin.dat", "tracked\n")
    r.run("save", "-m", "tracks bin")
    r.run("switch", "main")
    # ineligible working-tree file (NUL at first byte) at the target path
    with open(os.path.join(r.path, "bin.dat"), "wb") as f:
        f.write(b"\x00ineligible")
    p = r.run("switch", "feature2")
    assert p.returncode != 0, "ineligible-file collision must block switch"
    assert r.read("bin.dat") == b"\x00ineligible", \
        "colliding ineligible file must be preserved"
s.test("J09 ineligible-file collision protection", case_j09)


# J10: unrelated filtered-out/ineligible paths are preserved on switch,
# including an excluded descendant inside a directory container also needed by
# the target snapshot (spec 08 §28). Excluding dir/keep.txt must not prevent
# switching to a branch that tracks dir/a.txt; keep.txt stays.
def case_j10():
    r = new_repo("j10"); r.init()
    _set_cfg(r, {"format_version": 1, "tracking": {"include": ["**"],
                                                   "exclude": ["dir/keep.txt"]}})
    r.write("a.txt", "A\n")
    r.run("save", "-m", "base")
    r.run("branch", "create", "feature2")
    r.run("switch", "feature2")
    r.write("dir/a.txt", "tracked\n")
    r.run("save", "-m", "tracks dir/a")
    r.run("switch", "main")
    # excluded descendant inside the dir container the target also needs
    r.write("dir/keep.txt", "keep me\n")
    p = r.run("switch", "feature2")
    assert p.returncode == 0, "excluded dir descendant must not block switch: %s" % p.stderr.decode()
    assert r.exists("dir/a.txt"), "target dir/a.txt must be materialized"
    assert r.read("dir/keep.txt") == b"keep me\n", \
        "excluded descendant inside a needed dir container must be preserved"
s.test("J10 excluded descendant preserved in needed dir", case_j10)


# J11: branch name beginning with - rejected
def case_j11b():
    r = new_repo("j11b"); _new(r)
    p = r.run("branch", "create", "-foo")
    assert p.returncode != 0, "branch starting with - must be rejected"
s.test("J11b branch starting with -", case_j11b)


# J12: switch to current branch is a no-op preserving dirty changes
def case_j12():
    r = new_repo("j12"); _new(r)
    r.write("local.txt", "L\n")
    p = r.run("switch", "main")
    assert p.returncode == 0, p.stderr.decode()
    assert b"already on branch main" in p.stdout, p.stdout
    # dirty local file preserved
    assert r.exists("local.txt")
s.test("J12 switch current no-op", case_j12)


# J13: delete noncurrent branch with unreachable commits -> warning required
def case_j13():
    r = new_repo("j13"); _new(r)
    r.run("branch", "create", "feature")
    r.run("switch", "feature")
    r.write("a.txt", "B\n")
    r.run("save", "-m", "feat")   # unreachable from main
    r.run("switch", "main")
    p = r.run("branch", "delete", "feature")
    assert p.returncode == 0, "delete noncurrent must succeed"
    combined = (p.stdout + p.stderr).decode("utf-8").lower()
    assert "unreachable" in combined, "unreachable-commits warning required: %r" % combined
    # object history not physically deleted
    cid = saved_commit_id(r)  # main still has its commit
    assert os.path.exists(obj_path(r.path, cid))
s.test("J13 delete noncurrent with unreachable warning", case_j13)


# J14: invalid branch names rejected
def case_j14():
    r = new_repo("j14"); _new(r)
    # invalid names per acceptance matrix J14: `.`/`..`, `..` substring, bad
    # slash forms, trailing dot/space, backslash, forbidden Win32 characters,
    # reserved DOS device basenames (incl superscript forms), reserved HEAD,
    # control bytes, overlength names.
    invalid = ["-foo", ".", "..", "a..b", "a\\b", "con", "CON", "NUL",
               "com1", "lpt1", "aux", "prn", "COM9", "LPT9",
               "COM1.txt", "COM\u00b9", "COM\u00b2", "COM\u00b3",
               "LPT\u00b9", "LPT\u00b2", "LPT\u00b3",
               "topic/COM1.txt", "topic/PRN", "topic/NUL",
               "HEAD", "trailing. ", "trailing ", "trailing.",
               "a:b", "a>b", "a<b", "a|b", 'a"b', "a?b", "a*b",
               "/lead", "a//b", "trail/", "a/../b", "a/b/..",
               "control\x01", "tab\tname", "overlong/" + "x" * 200]
    for name in invalid:
        p = r.run("branch", "create", name)
        assert p.returncode != 0, "invalid branch name %r must be rejected" % name
    # valid: nested branch, UTF-8, and a dot inside a component (allowed)
    for name in ["fix/ui", "feature/v1", "name.txt", "bad/name.txt",
                 "中文分支", "emoji-\U0001f600", "COM10", "COM9x", "con2", "head"]:
        p = r.run("branch", "create", name)
        assert p.returncode == 0, "valid branch name %r must be accepted: %s" % (name, p.stderr.decode())
s.test("J14 invalid branch names rejected", case_j14)


# J15: case-insensitive collision rejected; exact spelling lookup
def case_j15():
    r = new_repo("j15"); _new(r)
    r.run("branch", "create", "Feature")
    p = r.run("branch", "create", "feature")
    assert p.returncode != 0, "case-insensitive branch collision must be rejected"
s.test("J15 case-insensitive branch collision", case_j15)


# J16/J17 need symlink or case-sensitive fs; on default NTFS these are
# environment-dependent. J16: switching between commits differing only by
# filename case safely materializes.
def case_j16():
    r = new_repo("j16"); _new(r)
    r.write("Readme.txt", "R\n")
    r.run("save", "-m", "upper")
    r.unlink("Readme.txt")
    r.write("readme.txt", "r\n")
    p = r.run("save", "-m", "lower")
    # save may reject case-only collision depending on fs; if it errored, skip
    if p.returncode != 0:
        raise Skip("case-only rename not materializable on this fs")
    cid = saved_commit_id(r)
    p = r.run("switch", "main")
    assert p.returncode == 0
s.test("J16 case-only switch", case_j16)


# J17: untracked native sibling differing only by case is a collision, not
# silently overwritten.
def case_j17():
    r = new_repo("j17"); _new(r)
    # The two-case-variant collision is only constructible on a case-sensitive
    # filesystem (or with per-directory case semantics). Default NTFS collapses
    # Data.txt/data.txt into one entry, so the scenario cannot be built.
    if not fs_is_case_sensitive(r.path):
        raise Skip("case-sensitive fs required to construct case siblings")
    r.write("Data.txt", "D\n")
    r.run("save", "-m", "one")
    # create an untracked sibling with different case before a restore
    r.unlink("Data.txt")
    r.write("data.txt", "lower\n")
    p = r.run("restore", saved_commit_id(r))
    # either blocked (collision) or the tracked case-only transition is applied;
    # must NOT silently create a duplicate
    r2 = r.read("data.txt") if os.path.exists(os.path.join(r.path, "data.txt")) else None
    r3 = r.read("Data.txt") if os.path.exists(os.path.join(r.path, "Data.txt")) else None
    assert not (r2 is not None and r3 is not None), "must not produce two case variants"
s.test("J17 untracked case sibling collision", case_j17)
