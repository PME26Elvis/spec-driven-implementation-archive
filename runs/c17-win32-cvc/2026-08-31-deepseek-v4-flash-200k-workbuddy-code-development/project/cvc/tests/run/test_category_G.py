# Category G - save / status behavior.
from testlib import *
import os, struct, time

s = suite("G-SaveAndStatus")


def _run_save(r, msg="m", **kw):
    return r.run("save", "-m", msg, **kw)


def _saved(r):
    return saved_commit_id(r)


# G01: first save is a root commit (no parent)
def case_g01():
    r = new_repo("g01"); r.init()
    r.write("a.txt", "hello\n")
    p = _run_save(r)
    assert p.returncode == 0, p.stderr.decode()
    cid = _saved(r)
    typ, cpay = read_loose_object(r.path, cid)
    assert typ == "commit"
    parent_count = cpay[32]
    assert parent_count == 0, "first commit must be a root (0 parents)"
s.test("G01 first save root commit", case_g01)


# G02: no-op save creates no new commit
def case_g02():
    r = new_repo("g02"); r.init()
    r.write("a.txt", "x\n")
    c1 = _run_save(r); assert c1.returncode == 0
    id1 = _saved(r)
    p = _run_save(r, "again")
    assert p.returncode == 0, p.stderr.decode()
    assert b"nothing to save" in p.stdout, "no-op save should say nothing to save: %r" % p.stdout
    id2 = _saved(r)
    assert id1 == id2, "no-op save must not create a commit"
s.test("G02 no-op save creates no commit", case_g02)


# G03: add/modify/delete status categories
def case_g03():
    r = new_repo("g03"); r.init()
    r.write("a.txt", "one\n"); r.write("b.txt", "two\n"); r.write("c.txt", "keep\n")
    _run_save(r)
    r.write("a.txt", "one modified\n")
    r.unlink("b.txt")
    r.write("d.txt", "new\n")
    p = r.run("status")
    assert b"modified     a.txt" in p.stdout, p.stdout
    assert b"deleted      b.txt" in p.stdout, p.stdout
    assert b"added        d.txt" in p.stdout, p.stdout
    # clean files not listed
    assert b"c.txt" not in p.stdout, p.stdout
s.test("G03 add/modify/delete status", case_g03)


# G04: type-changed category. On a host without symlinks, file<->dir and
# file<->symlink changes surface as delete+add, so this case is environment-
# dependent; the category is exercised where a symlink kind is available.
def case_g04():
    if not require_symlink_support():
        raise Skip("host denies symlink creation; type-changed surfaces as delete+add")
    r = new_repo("g04"); r.init()
    r.write("target.txt", "T\n")
    r.write("it", "regular\n")
    _run_save(r)
    os.unlink(os.path.join(r.path, "it"))
    os.symlink("target.txt", os.path.join(r.path, "it"))
    p = r.run("status")
    assert b"type-changed" in p.stdout, p.stdout
s.test("G04 type-changed category", case_g04)


# G05: ignored summary on save/status
def case_g05():
    r = new_repo("g05"); r.init()
    # a NUL-first file is ineligible -> ignored
    r.write("bad.bin", b"\x00data")
    p = _run_save(r)
    assert p.returncode == 0, p.stderr.decode()
    assert b"nothing to save" in p.stdout and b"ignored" in p.stdout, \
        "save should report ignored count: %r" % p.stdout
s.test("G05 ignored summary", case_g05)


# G06: save stores the complete selected snapshot (all files + tree + commit)
def case_g06():
    r = new_repo("g06"); r.init()
    r.write("top.txt", "top\n")
    r.write("sub/one.txt", "1\n")
    r.write("sub/two.txt", "2\n")
    p = _run_save(r)
    assert p.returncode == 0, p.stderr.decode()
    cid = _saved(r)
    typ, cpay = read_loose_object(r.path, cid)
    root = cpay[:32].hex()
    ttyp, tpay = read_loose_object(r.path, root)
    assert ttyp == "tree"
    # root has top.txt + sub; sub subtree has one.txt + two.txt
    assert b"top.txt" in tpay and b"sub" in tpay
    # every blob exists
    for name, content in (("top.txt", "top\n"), ("one.txt", "1\n"), ("two.txt", "2\n")):
        bid = oid(obj_envelope("blob", content.encode()))
        assert os.path.exists(obj_path(r.path, bid)), "blob %s missing" % name
s.test("G06 complete snapshot stored", case_g06)


# G07: tracked-to-binary transition removes the entry but leaves the physical file
def case_g07():
    r = new_repo("g07"); r.init()
    r.write("f.txt", "text\n")
    _run_save(r)
    r.write("f.txt", b"\x00binary")
    p = r.run("status")
    assert b"deleted" in p.stdout, "binary-transitioned file should show as removed: %r" % p.stdout
    _run_save(r, "second")
    # physical file still present
    assert r.exists("f.txt"), "physical file must remain"
s.test("G07 tracked-to-binary transition", case_g07)


# G08: filter removal removes path from snapshot but leaves physical file
def case_g08():
    r = new_repo("g08"); r.init()
    r.write("keep.txt", "keep\n"); r.write("drop.txt", "drop\n")
    _run_save(r)
    # restrict tracking to keep.txt only -> drop.txt no longer tracked
    cfg = '{"format_version":1,"tracking":{"include":["keep.txt"]}}'
    with open(os.path.join(r.path, ".cvc", "config.json"), "w") as f:
        f.write(cfg)
    p = r.run("status")
    assert b"deleted      drop.txt" in p.stdout, p.stdout
    _run_save(r, "second")
    # physical file remains
    assert r.exists("drop.txt"), "physical drop.txt must remain"
s.test("G08 filter removal", case_g08)


# G09: long / UTF-8 commit message
def case_g09():
    r = new_repo("g09"); r.init()
    r.write("a.txt", "x\n")
    msg = "这是一条很长的提交信息 " + "x" * 500
    p = _run_save(r, msg)
    assert p.returncode == 0, p.stderr.decode()
    p = r.run("log")
    assert msg.encode("utf-8") in p.stdout, "log must contain the full UTF-8 message"
s.test("G09 long UTF-8 message", case_g09)


# G10: save.show_diffstat=false suppresses diffstat but preserves snapshot
def case_g10():
    r = new_repo("g10"); r.init()
    r.write("a.txt", "x\n")
    _run_save(r)
    cfg = '{"format_version":1,"save":{"show_diffstat":false}}'
    with open(os.path.join(r.path, ".cvc", "config.json"), "w") as f:
        f.write(cfg)
    r.write("a.txt", "y\n")
    p = _run_save(r, "second")
    assert p.returncode == 0, p.stderr.decode()
    assert b"a.txt:" not in p.stdout, "diffstat must be suppressed: %r" % p.stdout
    assert b"saved" in p.stdout, "save must still report the saved id"
    # snapshot preserved
    cid = _saved(r)
    typ, cpay = read_loose_object(r.path, cid)
    root = cpay[:32].hex()
    ttyp, tpay = read_loose_object(r.path, root)
    assert b"a.txt" in tpay
s.test("G10 show_diffstat=false", case_g10)


# G11: empty commit message rejected where -m is required
def case_g11():
    r = new_repo("g11"); r.init()
    r.write("a.txt", "x\n")
    p = _run_save(r, "")
    assert p.returncode != 0, "empty -m must be rejected"
    p2 = r.run("save")
    assert p2.returncode != 0, "missing -m must be rejected"
s.test("G11 empty message rejected", case_g11)
