# Category K - Revision resolution and restore.
from testlib import *
import os

s = suite("K-RevisionRestore")


def _base(r):
    r.init()
    r.write("a.txt", "A\n")
    r.run("save", "-m", "base")
    return saved_commit_id(r)


# K01: full commit ID resolves
def case_k01():
    r = new_repo("k01"); cid = _base(r)
    p = r.run("diff", cid)
    assert p.returncode == 0, p.stderr.decode()
s.test("K01 full commit ID", case_k01)


# K02: branch revision resolves
def case_k02():
    r = new_repo("k02"); _base(r)
    r.run("branch", "create", "feature")
    p = r.run("diff", "feature")
    assert p.returncode == 0, p.stderr.decode()
s.test("K02 branch revision", case_k02)


# K03: unique >=8-char prefix resolves
def case_k03():
    r = new_repo("k03"); cid = _base(r)
    p = r.run("diff", cid[:8])
    assert p.returncode == 0, "unique 8-char prefix must resolve: %s" % p.stderr.decode()
    p = r.run("diff", cid[:20])
    assert p.returncode == 0, "unique 20-char prefix must resolve"
s.test("K03 unique >=8-char prefix", case_k03)


# K04: ambiguous prefix rejected
def case_k04():
    r = new_repo("k04"); _base(r)
    # two commits sharing >=8 prefix is impossible with real hashes, but a too-short
    # prefix (<8) is rejected; also verify a nonexistent prefix fails cleanly.
    cid = saved_commit_id(r)
    p = r.run("diff", cid[:4])
    assert p.returncode != 0, "prefix <8 hex must be rejected"
    p = r.run("diff", "deadbeef00")
    assert p.returncode != 0, "unknown prefix must fail"
s.test("K04 ambiguous/too-short prefix", case_k04)


# K05: restore regular file bytes
def case_k05():
    r = new_repo("k05"); cid = _base(r)
    r.write("a.txt", "CHANGED\n")
    p = r.run("restore", "a.txt", "--from", cid)
    assert p.returncode == 0, p.stderr.decode()
    assert r.read("a.txt") == b"A\n", "restore must bring back original bytes"
s.test("K05 restore regular file", case_k05)


# K06: restore Windows symbolic link with target and file/dir kind (needs symlinks)
def case_k06():
    if not require_symlink_support():
        raise Skip("symlink support unavailable on this host")
    r = new_repo("k06"); _base(r)
    os.symlink("target.txt", os.path.join(r.path, "link.txt"), target_is_directory=False)
    r.run("save", "-m", "add link")
    cid = saved_commit_id(r)
    os.remove(os.path.join(r.path, "link.txt"))
    p = r.run("restore", "link.txt", "--from", cid)
    assert p.returncode == 0, p.stderr.decode()
    assert os.path.islink(os.path.join(r.path, "link.txt")), "symlink must be restored"
    assert os.readlink(os.path.join(r.path, "link.txt")) == "target.txt"
s.test("K06 restore symbolic link", case_k06)


# K07: recursive directory restore
def case_k07():
    r = new_repo("k07"); _base(r)
    r.write("dir/x.txt", "X\n")
    r.write("dir/y.txt", "Y\n")
    r.run("save", "-m", "dir")
    cid = saved_commit_id(r)
    r.write("dir/x.txt", "X2\n")
    r.run("restore", "dir", "--from", cid)
    assert r.read("dir/x.txt") == b"X\n"
    assert r.read("dir/y.txt") == b"Y\n"
s.test("K07 recursive directory restore", case_k07)


# K08: absent revision path fails safely (leaves working tree unchanged)
def case_k08():
    r = new_repo("k08"); _base(r)
    r.write("keep.txt", "K\n")
    cid = saved_commit_id(r)
    r.write("keep.txt", "EDITED\n")
    p = r.run("restore", "nonexistent.txt", "--from", cid)
    assert p.returncode != 0, "absent path must fail"
    assert r.read("keep.txt") == b"EDITED\n", "working tree must be unchanged"
s.test("K08 absent revision path fails", case_k08)


# K09: recursive dir restore removes current-HEAD descendants absent from source
# while preserving unrelated untracked descendants
def case_k09():
    r = new_repo("k09"); _base(r)
    r.write("proj/f1.txt", "F1\n")
    r.run("save", "-m", "has f1")
    cid1 = saved_commit_id(r)
    r.write("proj/f2.txt", "F2\n")
    r.run("save", "-m", "adds f2")
    cid2 = saved_commit_id(r)
    # untracked file inside proj that is not tracked by HEAD
    r.write("proj/untracked.txt", "U\n")
    p = r.run("restore", "proj", "--from", cid1)
    assert p.returncode == 0, p.stderr.decode()
    assert r.exists("proj/f1.txt"), "f1 present in source must remain"
    assert not r.exists("proj/f2.txt"), "f2 absent from source must be removed"
    assert r.exists("proj/untracked.txt"), "unrelated untracked descendant must be preserved"
s.test("K09 recursive dir restore removes absent tracked descendants", case_k09)


# K10: restore collision/failure leaves requested subtree unchanged
def case_k10():
    r = new_repo("k10"); _base(r)
    r.write("dir/tracked.txt", "T\n")
    r.run("save", "-m", "dir")
    cid = saved_commit_id(r)
    # place an untracked dir where source needs a file -> collision -> restore fails
    r.unlink("dir/tracked.txt")
    os.makedirs(os.path.join(r.path, "dir", "tracked.txt"))  # dir where file should be
    p = r.run("restore", "dir", "--from", cid)
    assert p.returncode != 0, "collision must fail"
    assert os.path.isdir(os.path.join(r.path, "dir", "tracked.txt")), "untracked dir must be preserved"
s.test("K10 restore collision leaves subtree unchanged", case_k10)


# K11: valid unreachable commit resolves by full ID; corrupt candidate causes integrity failure
def case_k11():
    r = new_repo("k11"); _base(r)
    r.run("branch", "create", "feat")
    r.run("switch", "feat")
    r.write("a.txt", "B\n")
    r.run("save", "-m", "feat commit")
    fcid = saved_commit_id(r)
    r.run("switch", "main")
    # feat is now a noncurrent branch; its commit is reachable via branch feat, so
    # delete feat to make the commit unreachable, then resolve by full ID.
    r.run("branch", "delete", "feat")
    p = r.run("diff", fcid)
    assert p.returncode == 0, "unreachable commit by full ID must resolve: %s" % p.stderr.decode()
    p = r.run("diff", fcid[:12])
    assert p.returncode == 0, "unreachable commit by unique prefix must resolve"
    # corrupt the canonical object path -> integrity failure
    op = obj_path(r.path, fcid)
    with open(op, "wb") as f:
        f.write(b"garbage")
    p = r.run("diff", fcid)
    assert p.returncode != 0, "corrupt candidate must cause integrity failure"
s.test("K11 unreachable resolve; corrupt candidate fails", case_k11)


# K12: root-relative path operands from subdirectories; leading '-' operand accepted
def case_k12():
    r = new_repo("k12"); _base(r)
    r.write("sub/nested.txt", "N\n")
    r.write("-notes.txt", "M\n")
    r.run("save", "-m", "add files")
    cid = saved_commit_id(r)
    r.write("-notes.txt", "M2\n")
    sub = os.path.join(r.path, "sub")
    os.makedirs(sub, exist_ok=True)
    # restore -notes.txt from a subdirectory; root-relative operand
    p = r.run("restore", "-notes.txt", "--from", cid, cwd=sub)
    assert p.returncode == 0, "leading-dash operand from subdir must restore: %s" % p.stderr.decode()
    assert r.read("-notes.txt") == b"M\n"
    # sub/nested.txt root-relative from a subdir refers to repo-root nested.txt
    r.write("nested.txt", "ROOT\n")
    p = r.run("restore", "sub/nested.txt", "--from", cid, cwd=sub)
    assert p.returncode == 0, "root-relative nested path must restore"
    assert r.read("sub/nested.txt") == b"N\n"
s.test("K12 root-relative from subdir; leading-dash operand", case_k12)


# K13: native absolute/drive paths, backslashes, empty/control operands,
# . /.. segments, and Windows-invalid/reserved components rejected
def case_k13():
    r = new_repo("k13"); _base(r)
    cid = saved_commit_id(r)
    drive = os.path.splitdrive(r.path)[0]
    bad = [
        os.path.join(r.path, "a.txt"),          # native absolute path
        drive + r"\a.txt",                      # drive path
        "a\\b.txt",                             # backslash
        "",                                     # empty
        "a\x01b",                               # control byte
        "a\x1fb",                               # control byte
        ".",                                    # . segment
        "..",                                   # .. segment
        "a/./b",                                # . segment inside
        "a/../b",                               # .. segment inside
        "CON",                                  # reserved device
        "a: b",                                 # forbidden colon
        "a*b",                                  # forbidden star
        "a?b",                                  # forbidden question
    ]
    for path in bad:
        p = r.run("restore", path, "--from", cid)
        assert p.returncode != 0, "invalid restore operand %r must be rejected" % path
s.test("K13 invalid path operands rejected", case_k13)
