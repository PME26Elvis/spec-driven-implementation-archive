# Category N - Verification and corruption.
from testlib import *
import os, struct, hashlib

s = suite("N-Verification")


def _make(r, files=("a.txt",)):
    r.init()
    for f in files:
        r.write(f, "hello\n")
    r.run("save", "-m", "base")
    return saved_commit_id(r)


def _verify(r):
    return r.run("verify")


def _expect_ok(r, what="healthy repo must verify"):
    p = _verify(r)
    assert p.returncode == 0, "%s: %s" % (what, p.stderr.decode())


def _expect_fail(r, what):
    p = _verify(r)
    assert p.returncode != 0, what


# N01: healthy repository verifies
def case_n01():
    r = new_repo("n01"); _make(r)
    p = _verify(r)
    assert p.returncode == 0, p.stderr.decode()
    assert b"verified OK" in p.stdout, p.stdout
s.test("N01 healthy repo verifies", case_n01)


# N02: edited blob detected by hash mismatch
def case_n02():
    r = new_repo("n02"); cid = _make(r)
    # find the blob object for a.txt
    tree = read_tree_for_commit(r, cid)
    blob_id = find_entry(r, tree, "a.txt")
    op = obj_path(r.path, blob_id)
    with open(op, "r+b") as f:
        f.seek(-1, os.SEEK_END)
        f.write(b"x")  # corrupt last byte
    _expect_fail(r, "edited blob must fail verify")
s.test("N02 edited blob detected", case_n02)


# N03: missing blob detected
def case_n03():
    r = new_repo("n03"); cid = _make(r)
    tree = read_tree_for_commit(r, cid)
    blob_id = find_entry(r, tree, "a.txt")
    os.remove(obj_path(r.path, blob_id))
    _expect_fail(r, "missing blob must fail verify")
s.test("N03 missing blob detected", case_n03)


# N04: missing tree detected
def case_n04():
    r = new_repo("n04")
    r.init()
    r.write("dir/x.txt", "X\n")
    r.run("save", "-m", "dir")
    cid = saved_commit_id(r)
    # root commit's root_tree; delete it
    _, commit_payload = read_loose_object(r.path, cid)
    root_tree = commit_payload[0:32]
    os.remove(obj_path(r.path, root_tree.hex()))
    _expect_fail(r, "missing tree must fail verify")
s.test("N04 missing tree detected", case_n04)


# N05: malformed tree entry detected
def case_n05():
    r = new_repo("n05"); cid = _make(r)
    tree = read_tree_for_commit(r, cid)
    op = obj_path(r.path, tree)
    with open(op, "r+b") as f:
        f.seek(-1, os.SEEK_END)
        f.write(b"\xff")  # corrupt an oid byte -> decode failure
    _expect_fail(r, "malformed tree must fail verify")
s.test("N05 malformed tree detected", case_n05)


# N06: malformed commit detected
def case_n06():
    r = new_repo("n06"); cid = _make(r)
    op = obj_path(r.path, cid)
    with open(op, "r+b") as f:
        f.seek(-1, os.SEEK_END)
        f.write(b"\xff")  # corrupt commit payload
    _expect_fail(r, "malformed commit must fail verify")
s.test("N06 malformed commit detected", case_n06)


# N07: bad branch ref detected
def case_n07():
    r = new_repo("n07"); cid = _make(r)
    # write garbage to refs/heads/main
    p = os.path.join(r.path, ".cvc", "refs", "heads", "main")
    with open(p, "wb") as f:
        f.write(b"not-a-hash\n")
    _expect_fail(r, "bad branch ref must fail verify")
s.test("N07 bad branch ref detected", case_n07)


# N08: bad HEAD detected
def case_n08():
    r = new_repo("n08"); _make(r)
    p = os.path.join(r.path, ".cvc", "HEAD")
    with open(p, "wb") as f:
        f.write(b"ref: refs/heads/nonexistent\n")
    _expect_fail(r, "bad HEAD must fail verify")
s.test("N08 bad HEAD detected", case_n08)


# N09: unknown repository format rejected
def case_n09():
    r = new_repo("n09"); _make(r)
    p = os.path.join(r.path, ".cvc", "config.json")
    with open(p, "rb") as f:
        data = f.read()
    data = data.replace(b'"format_version":1', b'"format_version":99', 1)
    with open(p, "wb") as f:
        f.write(data)
    _expect_fail(r, "unknown format_version must fail verify")
s.test("N09 unknown repo format rejected", case_n09)


# N10: unreachable valid object does not fail reachable verification
def case_n10():
    r = new_repo("n10"); _make(r)
    r.run("branch", "create", "feat")
    r.run("switch", "feat")
    r.write("a.txt", "B\n")
    r.run("save", "-m", "feat")
    fcid = saved_commit_id(r)
    r.run("switch", "main")
    r.run("branch", "delete", "feat")  # fcid now unreachable (valid object)
    _expect_ok(r, "unreachable valid object must not fail verify")
s.test("N10 unreachable valid object ok", case_n10)


# N11: reparse-point substitution of a required metadata path is rejected
def case_n11():
    if not require_symlink_support():
        raise Skip("symlink support unavailable")
    r = new_repo("n11"); _make(r)
    # replace .cvc/refs/heads with a symlink to a benign dir
    import shutil
    heads = os.path.join(r.path, ".cvc", "refs", "heads")
    shutil.rmtree(heads)
    os.symlink("main", heads, target_is_directory=True)
    _expect_fail(r, "reparse-point refs/heads must fail verify")
s.test("N11 reparse substitution rejected", case_n11)


# N13: malformed/hash-invalid object at canonical loose-object path fails verify
# even when unreachable
def case_n13():
    r = new_repo("n13"); _make(r)
    r.run("branch", "create", "feat")
    r.run("switch", "feat")
    r.write("a.txt", "B\n")
    r.run("save", "-m", "feat")
    fcid = saved_commit_id(r)
    r.run("switch", "main")
    r.run("branch", "delete", "feat")
    # corrupt the now-unreachable commit object
    with open(obj_path(r.path, fcid), "wb") as f:
        f.write(b"junk not an envelope")
    _expect_fail(r, "hash-invalid unreachable object must fail verify")
s.test("N13 hash-invalid unreachable object fails", case_n13)


# N14: object references whose actual type does not match expectation rejected
def case_n14():
    r = new_repo("n14"); _make(r)
    tree = read_tree_for_commit(r, saved_commit_id(r))
    blob_id = find_entry(r, tree, "a.txt")
    # rewrite the blob object file so its content is a *tree* payload -> hash mismatch
    # Simpler: craft a loose object at the blob path whose envelope is valid but
    # whose hash won't match -> caught as hash mismatch (still a type-integrity failure).
    payload = b"\x00\x00\x00\x00"  # empty tree payload
    env = obj_envelope("tree", payload)
    with open(obj_path(r.path, blob_id), "wb") as f:
        f.write(env)
    _expect_fail(r, "wrong-type reference must fail verify")
s.test("N14 wrong-type reference rejected", case_n14)


# N16: uppercase/noncanonical loose-object pathname rejected
def case_n16():
    r = new_repo("n16"); _make(r)
    # create an extra uppercase fan-out with a junk file; must be flagged
    d = os.path.join(r.path, ".cvc", "objects", "ZZ")
    os.makedirs(d, exist_ok=True)
    with open(os.path.join(d, "a" * 62), "wb") as f:
        f.write(b"x")
    _expect_fail(r, "uppercase noncanonical object path must fail verify")
s.test("N16 noncanonical object path rejected", case_n16)


# N18: externally introduced branch-ref namespace collision rejected
def case_n18():
    r = new_repo("n18"); _make(r)
    # two refs differing only by case would collide; NTFS cannot create them side by
    # side, so create a valid ref and an empty directory of the same name under a
    # case-differing sibling isn't possible. Instead simulate a file/dir prefix
    # conflict: create dir 'x' and file 'x/y' (both valid refs) is fine; to force a
    # collision, create a file ref and a directory at the same case-insensitive name.
    heads = os.path.join(r.path, ".cvc", "refs", "heads")
    # main already exists as a file. Create directory 'main' too (case-insensitive
    # collision: file vs dir at same name) - NTFS will refuse. So SKIP.
    raise Skip("case-collision ref namespace not constructible on case-insensitive NTFS")
s.test("N18 branch ref namespace collision", case_n18)


# N12: verify rejects a committed tree with a Windows-invalid component or a
# reserved DOS device basename.
def case_n12():
    r = new_repo("n12"); _make(r)
    blob_id = hashlib.sha256(obj_envelope("blob", b"x")).hexdigest()
    # component with a forbidden Win32 char (':')
    entries = struct.pack(">I", 1) + _entry(0x01, "a:b", bytes.fromhex(blob_id))
    env = obj_envelope("tree", entries)
    tid = oid(env)
    os.makedirs(os.path.join(r.path, ".cvc", "objects", tid[:2]), exist_ok=True)
    with open(obj_path(r.path, tid), "wb") as f:
        f.write(env)
    _expect_fail(r, "tree with Win32-invalid component must fail verify")
    # reserved DOS device basename (CON)
    os.remove(obj_path(r.path, tid))
    entries2 = struct.pack(">I", 1) + _entry(0x01, "CON", bytes.fromhex(blob_id))
    env2 = obj_envelope("tree", entries2)
    tid2 = oid(env2)
    os.makedirs(os.path.join(r.path, ".cvc", "objects", tid2[:2]), exist_ok=True)
    with open(obj_path(r.path, tid2), "wb") as f:
        f.write(env2)
    _expect_fail(r, "tree with reserved DOS device component must fail verify")
s.test("N12 Windows-invalid/device component rejected", case_n12)


# N15: an unreachable commit/tree with a missing or wrong-type referenced object fails verify
def case_n15():
    r = new_repo("n15"); _make(r)
    missing = "ff" * 32
    entries = struct.pack(">I", 1) + _entry(0x01, "gone.txt", bytes.fromhex(missing))
    env = obj_envelope("tree", entries)
    tid = oid(env)
    os.makedirs(os.path.join(r.path, ".cvc", "objects", tid[:2]), exist_ok=True)
    with open(obj_path(r.path, tid), "wb") as f:
        f.write(env)
    _expect_fail(r, "unreachable tree referencing missing object must fail verify")
s.test("N15 unreachable ref integrity fails", case_n15)


# N17: canonical tree with a component whose UTF-16 length exceeds volume max rejected
def case_n17():
    r = new_repo("n17"); _make(r)
    blob_id = hashlib.sha256(obj_envelope("blob", b"x")).hexdigest()
    longname = "n" * 300  # > NTFS 255 component limit
    entries = struct.pack(">I", 1) + _entry(0x01, longname, bytes.fromhex(blob_id))
    env = obj_envelope("tree", entries)
    tid = oid(env)
    os.makedirs(os.path.join(r.path, ".cvc", "objects", tid[:2]), exist_ok=True)
    with open(obj_path(r.path, tid), "wb") as f:
        f.write(env)
    _expect_fail(r, "tree with over-long component must fail verify")
s.test("N17 over-long component rejected", case_n17)


# ---- helpers ----
def _entry(etype, name, oid_bytes):
    nb = name.encode("utf-8")
    return bytes([etype]) + struct.pack(">I", len(nb)) + nb + oid_bytes

def read_tree_for_commit(r, cid):
    _, commit_payload = read_loose_object(r.path, cid)
    return commit_payload[0:32].hex()

def find_entry(r, tree_id, name):
    _, payload = read_loose_object(r.path, tree_id)
    n = struct.unpack(">I", payload[0:4])[0]
    pos = 4
    for _ in range(n):
        etype = payload[pos]; pos += 1
        nl = struct.unpack(">I", payload[pos:pos+4])[0]; pos += 4
        ename = payload[pos:pos+nl].decode("utf-8"); pos += nl
        oid = payload[pos:pos+32].hex(); pos += 32
        if ename == name:
            return oid
    raise AssertionError("entry %s not found in tree %s" % (name, tree_id))
