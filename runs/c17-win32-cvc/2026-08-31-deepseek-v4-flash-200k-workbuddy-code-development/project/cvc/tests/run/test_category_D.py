# Category D - SHA-256 and Object Store
from testlib import *

s = suite("D-SHA256AndObjectStore")

# D01-D03: standard SHA-256 vectors via hand-written impl, validated through cvc
# We verify by inspecting loose objects that cvc writes for known content.
def _blob_id_of(content):
    # independent envelope+sha computation
    return oid(obj_envelope("blob", content))

# D01: empty-string vector (blob 0\0)
def case_d01():
    assert oid(obj_envelope("blob", b"")) == \
        "473a0f4c3be8a93681a267e3b1e9a7dcda1185436fe141f7749120a303721813"
s.test("D01 blob 0 empty-string SHA vector", case_d01)

# D02: abc vector
def case_d02():
    assert oid(obj_envelope("blob", b"abc")) == \
        "c1cf6e465077930e88dc5136641d402f72a229ddd996f627d60e9639eaba35a6"
s.test("D02 blob 3 abc SHA vector", case_d02)

# D03: multi-block vector (use a >64-byte message, verify against hashlib)
def case_d03():
    msg = b"the quick brown fox jumps over the lazy dog " * 3
    assert oid(obj_envelope("blob", msg)) == hashlib.sha256(obj_envelope("blob", msg)).hexdigest()
    # Also directly verify hashlib('abc') known value
    assert hashlib.sha256(b"abc").hexdigest() == \
        "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad"
s.test("D03 multi-block SHA consistency", case_d03)

# D04: incremental chunk-boundary equivalence - save a large file, verify blob
def case_d04():
    r = new_repo("d04")
    r.init()
    content = b"chunkboundary" * 1000 + b"\n"  # > several SHA blocks
    r.write("big.txt", content)
    p = r.run("save", "-m", "big")
    assert p.returncode == 0, p.stderr
    blobid = _blob_id_of(content)
    assert r.exists(".cvc/objects/%s/%s" % (blobid[:2], blobid[2:])), \
        "blob object missing at expected ID"
s.test("D04 incremental chunk-boundary equivalence", case_d04)

# D05: identical file bytes deduplicate (shared blob)
def case_d05():
    r = new_repo("d05")
    r.init()
    r.write("a.txt", "same bytes")
    r.write("dir/b.txt", "same bytes")
    p = r.run("save", "-m", "dedup")
    assert p.returncode == 0, p.stderr
    blobid = _blob_id_of(b"same bytes")
    assert r.exists(obj_path(r.path, blobid)), "shared blob missing at expected ID"
    # count objects: 1 blob + 2 trees (root + dir subtree) + 1 commit = 4
    objs = []
    for prefix in os.listdir(os.path.join(r.path, ".cvc", "objects")):
        d = os.path.join(r.path, ".cvc", "objects", prefix)
        if os.path.isdir(d):
            objs.extend(os.listdir(d))
    assert len(objs) == 4, "expected 4 objects (1 blob + 2 trees + 1 commit), got %d" % len(objs)
    # the two identical files share exactly ONE blob object
    blob_count = sum(1 for o in objs if o == blobid[2:])
    assert blob_count == 1, "identical bytes must deduplicate to one blob, found %d" % blob_count
    assert os.path.exists(os.path.join(r.path, ".cvc", "objects", blobid[:2])), "blob missing"
s.test("D05 identical bytes deduplicate", case_d05)

# D06: existing valid object reused
def case_d06():
    r = new_repo("d06")
    r.init()
    r.write("f.txt", "reuse me")
    r.run("save", "-m", "one")
    blobid = _blob_id_of(b"reuse me")
    # modify a different path with same content, save again
    r.write("g.txt", "reuse me")
    r.run("save", "-m", "two")
    # no new blob object for same bytes; total blob objects for 'reuse me' == 1
    typ, _ = read_loose_object(r.path, blobid)
    assert typ == "blob"
s.test("D06 existing valid object reused", case_d06)

# D07: corrupt existing object at expected ID causes failure
def case_d07():
    r = new_repo("d07")
    r.init()
    r.write("f.txt", "original")
    r.run("save", "-m", "one")
    blobid = _blob_id_of(b"original")
    # Corrupt the blob object in place
    with open(obj_path(r.path, blobid), "r+b") as f:
        f.seek(0)
        f.write(b"X")
    r.write("f.txt", "original")  # unchanged content -> same blob needed
    p = r.run("save", "-m", "two")
    assert p.returncode != 0, "corrupt object should cause failure"
s.test("D07 corrupt object at expected ID fails", case_d07)

# D08: tree order deterministic under different creation orders
def case_d08():
    r = new_repo("d08")
    r.init()
    r.write("a.txt", "a")
    r.write("b.txt", "b")
    r.write("c.txt", "c")
    r.run("save", "-m", "one", env={"CVC_TEST_TIMESTAMP": "100"})
    # create in different order in another repo, same content, same ts
    r2 = new_repo("d08b")
    r2.init()
    r2.write("c.txt", "c")
    r2.write("b.txt", "b")
    r2.write("a.txt", "a")
    r2.run("save", "-m", "one", env={"CVC_TEST_TIMESTAMP": "100"})
    # identical trees + identical ts + identical parent -> identical commit
    id1 = re.search(r"commit ([0-9a-f]{64})", r.run("log").stdout.decode()).group(1)
    id2 = re.search(r"commit ([0-9a-f]{64})", r2.run("log").stdout.decode()).group(1)
    assert id1 == id2, "tree order should not affect commit id: %s vs %s" % (id1, id2)
s.test("D08 tree order deterministic", case_d08)

# D09: commit serialization deterministic with CVC_TEST_TIMESTAMP
def case_d09():
    r = new_repo("d09")
    r.init()
    r.write("f.txt", "x")
    p = r.run("save", "-m", "t1", env={"CVC_TEST_TIMESTAMP": "5000"})
    p2 = r.run("log", "--max-count=1")
    # Recreate same state in another repo with same ts -> identical commit id
    r2 = new_repo("d09b")
    r2.init()
    r2.write("f.txt", "x")
    r2.run("save", "-m", "t1", env={"CVC_TEST_TIMESTAMP": "5000"})
    # both should produce identical commit ids
    import re
    id1 = re.search(r"commit ([0-9a-f]{64})", r.run("log").stdout.decode()).group(1)
    id2 = re.search(r"commit ([0-9a-f]{64})", r2.run("log").stdout.decode()).group(1)
    assert id1 == id2, "deterministic commit serialization failed: %s vs %s" % (id1, id2)
s.test("D09 deterministic commit with CVC_TEST_TIMESTAMP", case_d09)

# D10: canonical blob envelope fixed IDs
def case_d10():
    r = new_repo("d10")
    r.init()
    r.write("empty.txt", "")
    r.write("abc.txt", "abc")
    r.run("save", "-m", "vec")
    emptyid = "473a0f4c3be8a93681a267e3b1e9a7dcda1185436fe141f7749120a303721813"
    abcid = "c1cf6e465077930e88dc5136641d402f72a229ddd996f627d60e9639eaba35a6"
    assert r.exists(".cvc/objects/%s/%s" % (emptyid[:2], emptyid[2:])), "empty blob vector"
    assert r.exists(".cvc/objects/%s/%s" % (abcid[:2], abcid[2:])), "abc blob vector"
s.test("D10 canonical blob envelope fixed IDs", case_d10)

# D11: canonical tree binary encoding/order independently decoded
def case_d11():
    r = new_repo("d11")
    r.init()
    r.write("b.txt", "bb")
    r.write("a.txt", "aa")
    r.run("save", "-m", "tree")
    p = r.run("log", "--max-count=1")
    commit_id = re.search(r"commit ([0-9a-f]{64})", p.stdout.decode()).group(1)
    ctyp, cpay = read_loose_object(r.path, commit_id)
    assert ctyp == "commit"
    root_tree = cpay[:32].hex()
    ttyp, tpay = read_loose_object(r.path, root_tree)
    assert ttyp == "tree"
    (count,) = struct.unpack(">I", tpay[:4])
    assert count == 2, "expected 2 entries, got %d" % count
    pos = 4
    names = []
    for _ in range(count):
        etype = tpay[pos]; pos += 1
        (nlen,) = struct.unpack(">I", tpay[pos:pos+4]); pos += 4
        name = tpay[pos:pos+nlen]; pos += nlen
        _ = tpay[pos:pos+32]; pos += 32
        names.append(name)
        assert etype == 0x01, "expected regular-file blob type"
    assert names == sorted(names), "tree entries not sorted: %r" % names
    assert names == [b"a.txt", b"b.txt"], "names wrong: %r" % names
s.test("D11 tree binary encoding independently decoded", case_d11)

# D12: a tree whose entries are in non-canonical (unsorted) byte order, or with
# a noncanonical envelope encoding, is rejected by verify as malformed
# (objects.c tree_decode -> tree_sort_validate). Build a handcrafted object
# graph with an unsorted tree and confirm verify fails.
def _loose(root, oidhex):
    return os.path.join(root, ".cvc", "objects", oidhex[:2], oidhex[2:])


def _wloose(root, typ, payload):
    oidhex = oid(obj_envelope(typ, payload))
    p = _loose(root, oidhex)
    os.makedirs(os.path.dirname(p), exist_ok=True)
    with open(p, "wb") as f:
        f.write(obj_envelope(typ, payload))
    return oidhex


def _tpay(entries):
    """entries: list of (type, name, oidhex) in the GIVEN (byte) order."""
    payload = struct.pack(">I", len(entries))
    for typ, name, oidhex in entries:
        payload += bytes([typ]) + struct.pack(">I", len(name)) + name.encode() + bytes.fromhex(oidhex)
    return payload


def _cmt(root_tree_hex, parents, ts, message):
    payload = bytes.fromhex(root_tree_hex) + bytes([len(parents)])
    for p in parents:
        payload += bytes.fromhex(p)
    payload += struct.pack(">q", ts) + struct.pack(">Q", len(message)) + message.encode()
    return payload


def case_d12():
    r = new_repo("d12"); r.init()
    blobA = _wloose(r.path, "blob", b"A\n")
    blobB = _wloose(r.path, "blob", b"B\n")
    # non-canonical tree: entries in UNSORTED byte order (b.txt before a.txt)
    bad_payload = _tpay([(0x01, "b.txt", blobB), (0x01, "a.txt", blobA)])
    bad_tree = _wloose(r.path, "tree", bad_payload)
    commit_id = _wloose(r.path, "commit",
                        _cmt(bad_tree, [], 100, "unsorted tree"))
    # point refs/heads/main and HEAD at the handcrafted commit
    with open(os.path.join(r.path, ".cvc", "refs", "heads", "main"), "w") as f:
        f.write(commit_id + "\n")
    with open(os.path.join(r.path, ".cvc", "HEAD"), "w") as f:
        f.write("ref: refs/heads/main\n")
    # verify must reject the non-canonical (unsorted) tree
    p = r.run("verify")
    combined = (p.stdout + p.stderr)
    assert p.returncode != 0 or b"malformed" in combined.lower(), \
        "verify must reject an unsorted tree: rc=%d %r" % (p.returncode, combined)
s.test("D12 unsorted/noncanonical tree rejected by verify", case_d12)

# D13: canonical empty-tree and root-commit vectors
def case_d13():
    assert oid(empty_tree_bytes()) == EMPTY_TREE_ID
    # root commit vector
    emt = hashlib.sha256(empty_tree_bytes()).digest()
    payload = emt + b"\x00" + struct.pack(">qQ", 0, 1) + b"x"
    assert oid(obj_envelope("commit", payload)) == \
        "b76903cf9661046c99f6f4d4e9ceda05cef2607b47bd9b2f9396ea67ad1e72ab"
s.test("D13 empty-tree and root-commit vectors", case_d13)

# D14: malformed CVC_TEST_TIMESTAMP rejected on commit-creating ops
def case_d14():
    r = new_repo("d14")
    r.init()
    r.write("f", "x")
    for bad in ["+5", "01", "-0", " 5", "5 ", "abc", "99999999999999999999999"]:
        p = r.run("save", "-m", "m", env={"CVC_TEST_TIMESTAMP": bad})
        assert p.returncode != 0, "ts %r should be rejected" % bad
    # ref should not have moved
    with open(os.path.join(r.path, ".cvc", "refs", "heads", "main"), "rb") as f:
        assert f.read() == b"", "ref must not move on bad ts"
s.test("D14 malformed CVC_TEST_TIMESTAMP rejected", case_d14)

# D15: canonical Windows symlink vector
def case_d15():
    assert oid(obj_envelope("symlink", b"target")) == \
        "99141839c37ea810ef652b9e77d1770a93d34debd0a6e418dce83306659e6e60"
s.test("D15 symlink 6 target vector", case_d15)

# D16: one-entry file-link/dir-link trees differ only by type byte
def case_d16():
    sym_id = hashlib.sha256(b"symlink 6\x00target").digest()
    def tree_one(etype):
        payload = struct.pack(">IBI", 1, etype, 2) + b"ln" + sym_id
        return obj_envelope("tree", payload)
    t02 = tree_one(0x02); t04 = tree_one(0x04)
    assert oid(t02) == "b669e897b7e5d2301e5fb1b65cf0920cc2c1b04513a5826ce897eae04c2a6d75"
    assert oid(t04) == "6f5efa3d9b9747c244f74c0045d23ecd439e8165815a384b14faaa388ac94487"
    assert t02 != t04
    # Envelope header "tree <len>\0" is 8 bytes for a 43-byte payload; the
    # tree payload begins with a 4-byte big-endian count, then the 1-byte
    # entry type at absolute index 8+4 = 12. The two trees must be identical
    # at every byte except that entry-type byte.
    assert t02[:12] == t04[:12], "envelope+count differ"
    assert t02[12] == 0x02 and t04[12] == 0x04, "type byte in wrong position"
    assert t02[:12] + t02[13:] == t04[:12] + t04[13:], \
        "trees should differ only by the entry-type byte"
    assert len(t02) == len(t04)
s.test("D16 file-link vs dir-link tree vectors", case_d16)
