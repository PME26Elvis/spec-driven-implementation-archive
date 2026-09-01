# Category H - glob / filtering semantics.
from testlib import *
import os, json

s = suite("H-GlobAndFiltering")


def _run_save(r, msg="m", **kw):
    return r.run("save", "-m", msg, **kw)


def _set_cfg(r, cfg):
    with open(os.path.join(r.path, ".cvc", "config.json"), "w") as f:
        json.dump(cfg, f)


# H01: * does not cross /
def case_h01():
    r = new_repo("h01"); r.init()
    r.write("a.c", "A\n"); r.write("src/b.c", "B\n")
    _set_cfg(r, {"format_version": 1, "tracking": {"include": ["*.c"]}})
    p = _run_save(r)
    assert p.returncode == 0, p.stderr.decode()
    # only a.c tracked (src/b.c not matched by *.c)
    cid = saved_commit_id(r)
    typ, cpay = read_loose_object(r.path, cid)
    ttyp, tpay = read_loose_object(r.path, cpay[:32].hex())
    assert b"a.c" in tpay
    assert b"src" not in tpay, "* must not cross /"
s.test("H01 * does not cross /", case_h01)


# H02: ? matches one non-separator byte
def case_h02():
    r = new_repo("h02"); r.init()
    r.write("a1.txt", "A\n"); r.write("a2.txt", "B\n"); r.write("ab12.txt", "C\n")
    _set_cfg(r, {"format_version": 1, "tracking": {"include": ["a?.txt"]}})
    p = _run_save(r)
    cid = saved_commit_id(r)
    typ, cpay = read_loose_object(r.path, cid)
    ttyp, tpay = read_loose_object(r.path, cpay[:32].hex())
    assert b"a1.txt" in tpay and b"a2.txt" in tpay
    assert b"ab12.txt" not in tpay, "? must match exactly one byte"
s.test("H02 ? one non-separator byte", case_h02)


# H03: ** crosses directories
def case_h03():
    r = new_repo("h03"); r.init()
    r.write("src/deep/nested.txt", "N\n"); r.write("top.txt", "T\n")
    _set_cfg(r, {"format_version": 1, "tracking": {"include": ["src/**"]}})
    p = _run_save(r)
    cid = saved_commit_id(r)
    typ, cpay = read_loose_object(r.path, cid)
    ttyp, tpay = read_loose_object(r.path, cpay[:32].hex())
    assert b"src" in tpay  # subtree present
    assert b"top.txt" not in tpay
s.test("H03 ** crosses dirs", case_h03)


# H04: **/*.md includes root-level markdown
def case_h04():
    r = new_repo("h04"); r.init()
    r.write("README.md", "R\n"); r.write("docs/guide.md", "G\n"); r.write("code.c", "C\n")
    _set_cfg(r, {"format_version": 1, "tracking": {"include": ["**/*.md"]}})
    p = _run_save(r)
    cid = saved_commit_id(r)
    typ, cpay = read_loose_object(r.path, cid)
    root = cpay[:32].hex()
    ttyp, tpay = read_loose_object(r.path, root)
    assert b"README.md" in tpay, "root-level md must match **/*.md"
    assert b"docs" in tpay
    assert b"code.c" not in tpay
s.test("H04 **/*.md root md", case_h04)


# H05: include then exclude precedence (exclude wins)
def case_h05():
    r = new_repo("h05"); r.init()
    r.write("keep.txt", "K\n"); r.write("build/skip.txt", "S\n")
    _set_cfg(r, {"format_version": 1, "tracking": {"include": ["**/*.txt"], "exclude": ["build/**"]}})
    p = _run_save(r)
    cid = saved_commit_id(r)
    typ, cpay = read_loose_object(r.path, cid)
    ttyp, tpay = read_loose_object(r.path, cpay[:32].hex())
    assert b"keep.txt" in tpay
    assert b"build" not in tpay, "exclude must win over include"
s.test("H05 include/exclude precedence", case_h05)


# H06: save --include replaces diffstat config list without changing tree
def case_h06():
    r = new_repo("h06"); r.init()
    r.write("a.txt", "A\n"); r.write("b.txt", "B\n")
    _run_save(r)  # commit baseline with both
    _set_cfg(r, {"format_version": 1, "diffstat": {"include": ["b.txt"]}})
    r.write("a.txt", "A2\n"); r.write("b.txt", "B2\n")
    p = r.run("save", "-m", "second", "--include=a.txt")
    assert p.returncode == 0, p.stderr.decode()
    assert b"a.txt:" in p.stdout, "CLI --include should select a.txt: %r" % p.stdout
    assert b"b.txt:" not in p.stdout, "CLI --include replaces config list"
    # both were saved (tree has both)
    cid = saved_commit_id(r)
    typ, cpay = read_loose_object(r.path, cid)
    ttyp, tpay = read_loose_object(r.path, cpay[:32].hex())
    assert b"a.txt" in tpay and b"b.txt" in tpay
s.test("H06 CLI list replaces diffstat config", case_h06)


# H07: malformed comma list rejected
def case_h07():
    r = new_repo("h07"); r.init()
    r.write("a.txt", "A\n")
    for bad in ("a,,b", "a,", ",a", "a,b,,"):
        p = r.run("save", "-m", "x", "--include=" + bad)
        assert p.returncode != 0, "malformed list %r must be rejected" % bad
s.test("H07 malformed comma list rejected", case_h07)


# H08: empty tracking include -> no tracked paths; empty diffstat include
# suppresses stats only; --include= rejected.
def case_h08():
    r = new_repo("h08"); r.init()
    r.write("a.txt", "A\n")
    _set_cfg(r, {"format_version": 1, "tracking": {"include": []}})
    p = _run_save(r)
    assert p.returncode == 0, p.stderr.decode()
    assert b"nothing to save" in p.stdout, "empty include -> nothing tracked: %r" % p.stdout
    # empty diffstat include suppresses stats only (tree still has file)
    _set_cfg(r, {"format_version": 1, "tracking": {"include": ["**/*.txt"]},
                 "diffstat": {"include": []}})
    p = _run_save(r, "second")
    assert p.returncode == 0, p.stderr.decode()
    assert b"saved" in p.stdout, p.stdout
    cid = saved_commit_id(r)
    typ, cpay = read_loose_object(r.path, cid)
    ttyp, tpay = read_loose_object(r.path, cpay[:32].hex())
    assert b"a.txt" in tpay, "empty diffstat include must not affect tree"
    # --include= rejected
    p = r.run("save", "-m", "x", "--include=")
    assert p.returncode != 0, "--include= must be rejected"
s.test("H08 empty include arrays", case_h08)


# H09: diffstat filter does not affect saved tree
def case_h09():
    r = new_repo("h09"); r.init()
    r.write("x.txt", "X\n"); r.write("y.txt", "Y\n")
    _set_cfg(r, {"format_version": 1, "diffstat": {"include": ["x.txt"]}})
    p = _run_save(r)
    assert p.returncode == 0, p.stderr.decode()
    cid = saved_commit_id(r)
    typ, cpay = read_loose_object(r.path, cid)
    ttyp, tpay = read_loose_object(r.path, cpay[:32].hex())
    assert b"x.txt" in tpay and b"y.txt" in tpay, "both tracked regardless of diffstat filter"
s.test("H09 diffstat filter not affect tree", case_h09)


# H10: Chinese exact-path pattern
def case_h10():
    r = new_repo("h10"); r.init()
    r.write("资料/报告.md", "中\n"); r.write("其他.md", "其\n")
    _set_cfg(r, {"format_version": 1, "tracking": {"include": ["资料/**"]}})
    p = _run_save(r)
    cid = saved_commit_id(r)
    typ, cpay = read_loose_object(r.path, cid)
    root = cpay[:32].hex()
    ttyp, tpay = read_loose_object(r.path, root)
    assert "资料".encode() in tpay, "Chinese dir subtree must be tracked"
    assert "其他.md".encode() not in tpay
s.test("H10 Chinese exact path", case_h10)


# H11: three-or-more consecutive stars rejected
def case_h11():
    r = new_repo("h11"); r.init()
    r.write("a.txt", "A\n")
    for pat in ("***", "a***b", "****"):
        p = r.run("save", "-m", "x", "--include=" + pat)
        assert p.returncode != 0, "pattern %r with 3+ stars must be rejected" % pat
s.test("H11 3+ consecutive stars rejected", case_h11)


# H12: save --include/--exclude never changes snapshot membership
def case_h12():
    r = new_repo("h12"); r.init()
    r.write("a.txt", "A\n"); r.write("b.txt", "B\n")
    p = r.run("save", "-m", "first", "--exclude=b.txt")
    assert p.returncode == 0, p.stderr.decode()
    # both a.txt and b.txt must be in the tree despite --exclude on save
    cid = saved_commit_id(r)
    typ, cpay = read_loose_object(r.path, cid)
    ttyp, tpay = read_loose_object(r.path, cpay[:32].hex())
    assert b"a.txt" in tpay and b"b.txt" in tpay, "save --exclude must not change tree"
s.test("H12 save filters never change membership", case_h12)


# H13: status/diff local filters don't inherit diffstat config and never alter
# tracking membership.
def case_h13():
    r = new_repo("h13"); r.init()
    r.write("a.txt", "A\n"); r.write("b.txt", "B\n")
    _set_cfg(r, {"format_version": 1, "diffstat": {"include": ["a.txt"]}})
    _run_save(r)
    r.write("a.txt", "A2\n"); r.write("b.txt", "B2\n")
    p = r.run("status")
    assert p.returncode == 0
    # status without --include shows both changes (does NOT inherit diffstat)
    assert b"a.txt" in p.stdout and b"b.txt" in p.stdout, p.stdout
s.test("H13 status/diff no diffstat inherit", case_h13)


# H14: CLI comma-list empty elements rejected; whitespace is literal
def case_h14():
    r = new_repo("h14"); r.init()
    r.write("a.txt", "A\n")
    for bad in ("a,,b", "a,", "a,b,,c"):
        p = r.run("save", "-m", "x", "--exclude=" + bad)
        assert p.returncode != 0, "empty element %r must be rejected" % bad
    # whitespace is literal pattern data (not trimmed); a pattern " a.txt" does
    # not match "a.txt"
    _set_cfg(r, {"format_version": 1, "tracking": {"include": [" a.txt"]}})
    p = _run_save(r)
    assert b"nothing to save" in p.stdout, "leading-space pattern must not match: %r" % p.stdout
s.test("H14 empty elements + whitespace literal", case_h14)


# H15: glob matching case-sensitive on UTF-8 bytes on Windows
def case_h15():
    r = new_repo("h15"); r.init()
    r.write("Readme.txt", "R\n")
    # case-sensitive: lowercase pattern must not match uppercase file
    _set_cfg(r, {"format_version": 1, "tracking": {"include": ["readme.txt"]}})
    p = _run_save(r)
    assert b"nothing to save" in p.stdout, "case-sensitive match expected: %r" % p.stdout
s.test("H15 case-sensitive glob", case_h15)
