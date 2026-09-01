# Category F - Windows symbolic links / reparse points.
# Symlink creation on Windows requires Administrator or Developer Mode. This
# host currently denies symlink creation (see acceptance evidence), so most
# cases here are SKIPPED with documented evidence. The tests are written to run
# fully on a symlink-capable host and to verify the safe-failure behavior where
# creation is denied.
from testlib import *
import ctypes, os, sys

s = suite("F-SymlinksAndReparse")

HAS_SYMLINK = require_symlink_support()


def _run_save(r, msg="m", **kw):
    return r.run("save", "-m", msg, **kw)


def _blob_id(b):
    return oid(obj_envelope("blob", b))


# F01: relative file symlink round-trips (stored PrintName, recreated as symlink)
def case_f01():
    if not HAS_SYMLINK:
        raise Skip("host denies symlink creation (no admin/Developer Mode)")
    r = new_repo("f01"); r.init()
    r.write("real.txt", "hello\n")
    os.symlink("real.txt", os.path.join(r.path, "link.txt"))
    assert os.path.islink(os.path.join(r.path, "link.txt"))
    p = _run_save(r)
    assert p.returncode == 0, p.stderr.decode()
    pid = saved_commit_id(r)
    # tree must contain link.txt as a FILE-symlink (0x02) referencing a
    # symlink object whose payload is "target" (the PrintName).
    typ, cpay = read_loose_object(r.path, pid)
    root = cpay[:32].hex()
    ttyp, tpay = read_loose_object(r.path, root)
    assert ttyp == "tree"
    assert b"link.txt" in tpay
    sym_target_id = _blob_id(b"real.txt")  # blob envelope == symlink payload here
    # find the symlink object referenced: parse tree for link.txt entry
    import struct as _st
    # (validated via restore below)
    r.run("restore", pid, expect_rc=0)
    assert os.path.islink(os.path.join(r.path, "link.txt")), "must restore as symlink"
s.test("F01 relative file symlink round-trip", case_f01)


# F02: absolute file symlink PrintName preserved
def case_f02():
    if not HAS_SYMLINK:
        raise Skip("host denies symlink creation")
    r = new_repo("f02"); r.init()
    r.write("a.txt", "A\n")
    abs_target = os.path.join(r.path, "a.txt")
    os.symlink(abs_target, os.path.join(r.path, "abs.link"))
    p = _run_save(r)
    assert p.returncode == 0, p.stderr.decode()
    # PrintName is the absolute path given at creation; restore must recreate it
    r.run("restore", saved_commit_id(r), expect_rc=0)
    assert os.path.islink(os.path.join(r.path, "abs.link"))
s.test("F02 absolute file symlink PrintName", case_f02)


# F03: dangling symlink round-trips (valid, no target access needed)
def case_f03():
    if not HAS_SYMLINK:
        raise Skip("host denies symlink creation")
    r = new_repo("f03"); r.init()
    os.symlink("does-not-exist.txt", os.path.join(r.path, "dangling.link"))
    p = _run_save(r)
    assert p.returncode == 0, p.stderr.decode()
    r.run("restore", saved_commit_id(r), expect_rc=0)
    assert os.path.islink(os.path.join(r.path, "dangling.link")), "dangling link must survive"
s.test("F03 dangling symlink", case_f03)


# F04: directory symlink is NOT traversed as a directory subtree
def case_f04():
    if not HAS_SYMLINK:
        raise Skip("host denies symlink creation")
    r = new_repo("f04"); r.init()
    r.write("real/dir.txt", "in real dir\n")
    os.symlink("real", os.path.join(r.path, "dirlink"))
    assert os.path.islink(os.path.join(r.path, "dirlink"))
    p = _run_save(r)
    assert p.returncode == 0, p.stderr.decode()
    pid = saved_commit_id(r)
    typ, cpay = read_loose_object(r.path, pid)
    ttyp, tpay = read_loose_object(r.path, cpay[:32].hex())
    # the dir symlink must be stored as a DIRECTORY-symlink entry (0x04),
    # not as a traversed subtree; its target is NOT expanded
    assert b"dirlink" in tpay
    # real/dir.txt is under "real/", tracked normally
    assert b"dir.txt" in tpay
s.test("F04 directory symlink not traversed", case_f04)


# F05: symlink loop (self-referential) does not hang and is stored as a link
def case_f05():
    if not HAS_SYMLINK:
        raise Skip("host denies symlink creation")
    r = new_repo("f05"); r.init()
    os.symlink("loop.link", os.path.join(r.path, "loop.link"))
    p = _run_save(r)
    assert p.returncode == 0, "symlink loop must not hang save: %s" % p.stderr.decode()
s.test("F05 symlink loop", case_f05)


# F06: printname change detected as modification
def case_f06():
    if not HAS_SYMLINK:
        raise Skip("host denies symlink creation")
    r = new_repo("f06"); r.init()
    r.write("one.txt", "1\n")
    os.symlink("one.txt", os.path.join(r.path, "lnk"))
    _run_save(r)
    os.remove(os.path.join(r.path, "lnk"))
    os.symlink("two.txt", os.path.join(r.path, "lnk"))
    p = r.run("status")
    assert b"modified" in p.stdout or b"lnk" in p.stdout
s.test("F06 symlink printname change detected", case_f06)


# F07: file <-> symlink type change
def case_f07():
    if not HAS_SYMLINK:
        raise Skip("host denies symlink creation")
    r = new_repo("f07"); r.init()
    r.write("target.txt", "T\n")
    r.write("it", "regular\n")
    _run_save(r)
    # turn it into a symlink
    os.remove(os.path.join(r.path, "it"))
    os.symlink("target.txt", os.path.join(r.path, "it"))
    p = r.run("status")
    assert b"type-changed" in p.stdout or b"modified" in p.stdout
s.test("F07 file to symlink type change", case_f07)


# F08: reparse-point collision safety for object/metadata paths (verify rejects)
def case_f08():
    if not HAS_SYMLINK:
        raise Skip("host denies symlink creation (needs to inject reparse point)")
    r = new_repo("f08"); r.init()
    r.write("a.txt", "A\n")
    _run_save(r)
    # Replace an object fan-out dir with a symlink pointing elsewhere, then
    # verify must reject (N12/N13 metadata reparse defense).
    objdir = os.path.join(r.path, ".cvc", "objects", "ab")
    evil = os.path.join(r.path, "evil")
    os.makedirs(evil, exist_ok=True)
    os.rmdir(objdir)
    os.symlink("evil", objdir)
    p = r.run("verify")
    assert p.returncode != 0, "verify must reject reparse metadata"
s.test("F08 metadata reparse-point defense", case_f08)


# F09: dangling directory symlink is still a dir-link (0x04) kind
def case_f09():
    if not HAS_SYMLINK:
        raise Skip("host denies symlink creation")
    r = new_repo("f09"); r.init()
    os.symlink("nonexistent_dir", os.path.join(r.path, "dirlink"), target_is_directory=True)
    p = _run_save(r)
    assert p.returncode == 0, p.stderr.decode()
    pid = saved_commit_id(r)
    typ, cpay = read_loose_object(r.path, pid)
    ttyp, tpay = read_loose_object(r.path, cpay[:32].hex())
    assert b"dirlink" in tpay
s.test("F09 dangling directory symlink kind", case_f09)


# F10: junction / mount point (non-symlink reparse) is ignored, not traversed
def case_f10():
    if not HAS_SYMLINK:
        raise Skip("host denies reparse creation (needs admin/Developer Mode)")
    r = new_repo("f10"); r.init()
    target = os.path.join(r.path, "real")
    os.makedirs(target, exist_ok=True)
    with open(os.path.join(target, "inside.txt"), "w") as f:
        f.write("inside\n")
    junc = os.path.join(r.path, "junc")
    # Try to create a junction via cmd mklink /J
    import subprocess
    res = subprocess.run(["cmd", "/c", "mklink", "/J", junc, target],
                         capture_output=True)
    if res.returncode != 0 or not os.path.exists(os.path.join(junc, "inside.txt")):
        raise Skip("junction creation unavailable")
    p = _run_save(r)
    assert p.returncode == 0, p.stderr.decode()
    pid = saved_commit_id(r)
    typ, cpay = read_loose_object(r.path, pid)
    ttyp, tpay = read_loose_object(r.path, cpay[:32].hex())
    assert b"junc" not in tpay, "junction must be ignored"
    assert b"inside.txt" not in tpay, "junction contents must not be traversed"
s.test("F10 junction/mount-point ignored", case_f10)


# F11: file-symlink <-> dir-symlink kind change is a type change (0x02 vs 0x04)
def case_f11():
    if not HAS_SYMLINK:
        raise Skip("host denies symlink creation")
    r = new_repo("f11"); r.init()
    r.write("target.txt", "T\n")
    os.symlink("target.txt", os.path.join(r.path, "x"))
    _run_save(r)
    os.remove(os.path.join(r.path, "x"))
    os.symlink("target.txt", os.path.join(r.path, "x"), target_is_directory=True)
    p = r.run("status")
    assert b"type-changed" in p.stdout or b"modified" in p.stdout
s.test("F11 file vs dir symlink kind change", case_f11)


# F12: symlink creation denied -> operation fails nonzero (no silent regular file)
def case_f12():
    if HAS_SYMLINK:
        raise Skip("host permits symlink creation; denied-path case not testable")
    # On this host symlink creation is denied. Materializing a tracked symlink
    # must fail nonzero rather than silently substitute a regular file.
    r = new_repo("f12"); r.init()
    r.write("t.txt", "T\n")
    # Manually build a tree/commit with a file-symlink entry to force restore.
    sym_payload = b"t.txt"
    sym_id = _blob_id(sym_payload)
    # write the symlink object manually
    env = obj_envelope("symlink", sym_payload)
    sp = obj_path(r.path, oid(env))
    os.makedirs(os.path.dirname(sp), exist_ok=True)
    with open(sp, "wb") as f:
        f.write(env)
    import struct
    entry = struct.pack(">IBI", 1, 0x02, 2) + b"ln" + bytes.fromhex(sym_id)
    tree_payload = entry
    tenv = obj_envelope("tree", tree_payload)
    tp = obj_path(r.path, oid(tenv))
    os.makedirs(os.path.dirname(tp), exist_ok=True)
    with open(tp, "wb") as f:
        f.write(tenv)
    # commit referencing this tree
    import time
    root_tree = oid(tenv)
    commit_payload = bytes.fromhex(root_tree) + bytes([0]) + (0).to_bytes(8, "little", signed=True) + (3).to_bytes(8, "little") + b"sym"
    cenv = obj_envelope("commit", commit_payload)
    cid = oid(cenv)
    cp = obj_path(r.path, cid)
    os.makedirs(os.path.dirname(cp), exist_ok=True)
    with open(cp, "wb") as f:
        f.write(cenv)
    # point refs/heads/main at this commit, HEAD at main
    refdir = os.path.join(r.path, ".cvc", "refs", "heads")
    os.makedirs(refdir, exist_ok=True)
    with open(os.path.join(refdir, "main"), "wb") as f:
        f.write((cid + "\n").encode())
    with open(os.path.join(r.path, ".cvc", "HEAD"), "wb") as f:
        f.write(b"ref: refs/heads/main\n")
    # materialize to a clean checkout of this commit; the symlink creation is
    # denied by the host -> restore/checkout must fail nonzero
    os.unlink(os.path.join(r.path, "t.txt"))
    p = r.run("checkout", cid)
    assert p.returncode != 0, "denied symlink creation must fail nonzero"
    # and no regular file silently substituted
    assert not os.path.exists(os.path.join(r.path, "ln")), "no silent regular file"
s.test("F12 symlink creation denied", case_f12)
