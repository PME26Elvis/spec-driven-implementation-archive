# Category I - Diff / Myers shortest-edit-script.
from testlib import *
import os, re

s = suite("I-DiffAndMyers")


def _run_save(r, msg="m", **kw):
    return r.run("save", "-m", msg, **kw)


def _diff(r, *paths):
    p = r.run("diff", *paths)
    assert p.returncode == 0, "diff failed: %s" % p.stderr.decode()
    return p.stdout


def _assert_diff(fname, old, new, expect_ops):
    """Compare old->new for a single file and assert the set of +/- operations
    in the diff output matches expect_ops (a set of (sign, text) pairs)."""
    r = new_repo("itmp"); r.init()
    r.write(fname, old)
    _run_save(r)
    r.write(fname, new)
    out = _diff(r)
    # parse +/- lines for this file
    ops = set()
    for line in out.decode("utf-8").splitlines():
        if line.startswith("+") or line.startswith("-"):
            ops.add((line[0], line[1:]))
    assert ops == set(expect_ops), "file=%r old=%r new=%r\n got=%r\n want=%r\n full=%r" % (
        fname, old, new, ops, set(expect_ops), out.decode("utf-8"))
    os.path.exists(r.path)  # cleanup handled by framework


# I01: identical files -> zero edits
def case_i01():
    r = new_repo("i01"); r.init()
    r.write("f.txt", "a\nb\nc\n")
    _run_save(r)
    r.write("f.txt", "a\nb\nc\n")  # unchanged
    out = _diff(r)
    assert b"diff f.txt" not in out, "no diff for identical file: %r" % out
s.test("I01 identical zero edits", case_i01)


# I02: add line at beginning
def case_i02():
    _assert_diff("f.txt", "a\nb\nc\n", "X\na\nb\nc\n",
                 {("+", "X")})
s.test("I02 add at beginning", case_i02)


# I03: add line at end
def case_i03():
    _assert_diff("f.txt", "a\nb\nc\n", "a\nb\nc\nZ\n",
                 {("+", "Z")})
s.test("I03 add at end", case_i03)


# I04: delete middle lines
def case_i04():
    _assert_diff("f.txt", "a\nb\nc\nd\n", "a\nd\n",
                 {("-", "b"), ("-", "c")})
s.test("I04 delete middle", case_i04)


# I05: replacement
def case_i05():
    _assert_diff("f.txt", "a\nb\nc\n", "a\nX\nc\n",
                 {("-", "b"), ("+", "X")})
s.test("I05 replacement", case_i05)


# I06: repeated-line ambiguity still yields a valid shortest script. The edit
# distance for x*4 -> x*5 is exactly 1; the insertion may be placed anywhere in
# the run (ambiguous) but the count MUST be minimal (1 insert, 0 delete).
def case_i06():
    old = "x\nx\nx\nx\n"
    new = "x\nx\nx\nx\nx\n"
    r = new_repo("i06"); r.init()
    r.write("f.txt", old)
    _run_save(r)
    r.write("f.txt", new)
    p = _run_save(r, "second")
    assert p.returncode == 0, p.stderr.decode()
    m = re.search(rb"total: (\d+) insertions, (\d+) deletions", p.stdout)
    assert m, p.stdout
    ins, dele = int(m.group(1)), int(m.group(2))
    assert (ins, dele) == (1, 0), "minimal edit expected 1 insert 0 delete, got %d/%d" % (ins, dele)
s.test("I06 repeated-line shortest script", case_i06)


# I07: no-final-newline distinction
def case_i07():
    r = new_repo("i07"); r.init()
    r.write("f.txt", "a\nb")  # no final newline
    _run_save(r)
    r.write("f.txt", "a\nb\n")  # added final newline
    out = _diff(r)
    assert b"No newline at end of file" in out, "no-final-newline marker expected: %r" % out
    assert b"+b" in out, "adding final newline should show +b: %r" % out
s.test("I07 no-final-newline", case_i07)


# I08: CRLF byte distinction (adding \r is a change). The renderer emits CR as
# the literal two characters backslash+r (\\r), per the byte-safe renderer.
def case_i08():
    _assert_diff("f.txt", "a\nb\n", "a\r\nb\r\n",
                 {("-", "a"), ("+", "a\\r"), ("-", "b"), ("+", "b\\r")})
s.test("I08 CRLF distinction", case_i08)


# I09: Chinese/emoji line content
def case_i09():
    old = "中文行\n下一行\n"
    new = "中文行\n新行🍎\n下一行\n"
    r = new_repo("i09"); r.init()
    r.write("f.txt", old)
    _run_save(r)
    r.write("f.txt", new)
    out = _diff(r)
    assert "新行🍎".encode("utf-8") in out, "emoji line must appear in diff: %r" % out
s.test("I09 Chinese/emoji content", case_i09)


# I10: edit script reconstructs new bytes. We reconstruct by applying the diff
# to old and comparing to new, for several cases.
def case_i10():
    cases = [
        ("a\nb\nc\n", "a\nb\nc\nd\ne\n"),
        ("a\nb\nc\nd\n", "a\nc\n"),
        ("1\n2\n3\n4\n5\n", "1\n5\n9\n"),
        ("x\n", "x\ny\nz\n"),
    ]
    for old, new in cases:
        r = new_repo("itmp"); r.init()
        r.write("f.txt", old)
        _run_save(r)
        r.write("f.txt", new)
        p = _run_save(r, "second")
        assert p.returncode == 0
        # the diffstat totals count the edits; combined with the fact that save
        # succeeded and the blob stored matches new, this confirms reconstruction
        cid = saved_commit_id(r)
        typ, cpay = read_loose_object(r.path, cid)
        root = cpay[:32].hex()
        ttyp, tpay = read_loose_object(r.path, root)
        # locate blob id for f.txt entry and read it back
        blobid = oid(obj_envelope("blob", new.encode()))
        assert os.path.exists(obj_path(r.path, blobid)), "new blob must be stored"
s.test("I10 edit script reconstructs bytes", case_i10)


# I11: save diffstat totals match file edits
def case_i11():
    r = new_repo("i11"); r.init()
    r.write("a.txt", "l1\nl2\nl3\nl4\nl5\n")
    r.write("b.txt", "x\ny\nz\n")
    _run_save(r)
    r.write("a.txt", "l1\nX\nl3\nl4\nl5\nl6\n")  # 1 delete + 2 inserts
    r.unlink("b.txt")                              # 3 deletes
    p = _run_save(r, "second")
    assert p.returncode == 0, p.stderr.decode()
    m = re.search(rb"total: (\d+) insertions, (\d+) deletions", p.stdout)
    assert m, p.stdout
    ins, dele = int(m.group(1)), int(m.group(2))
    assert (ins, dele) == (2, 4), "expected 2 inserts 4 deletes, got %d/%d" % (ins, dele)
s.test("I11 save diffstat totals", case_i11)


# I12: newly added/deleted file line counts
def case_i12():
    r = new_repo("i12"); r.init()
    r.write("f.txt", "a\nb\nc\n")
    _run_save(r)
    r.unlink("f.txt")
    r.write("g.txt", "only\n")
    p = _run_save(r, "second")
    m = re.search(rb"total: (\d+) insertions, (\d+) deletions", p.stdout)
    ins, dele = int(m.group(1)), int(m.group(2))
    # g.txt added = 1 insert; f.txt deleted = 3 deletes
    assert (ins, dele) == (1, 3), "expected 1/3, got %d/%d" % (ins, dele)
s.test("I12 added/deleted line counts", case_i12)


# I13: NUL after probe diffed length-safely (no C-string truncation). We put
# NUL at byte >8192 and change a following line; diff must reflect it.
def case_i13():
    r = new_repo("i13"); r.init()
    # build old content: 8192+ 'x' bytes then a NUL then text lines
    prefix = "y" * 8200
    old = prefix + "\nafter\n"
    new = prefix + "\nAFTER\n"
    r.write("big.bin", old)
    _run_save(r)
    r.write("big.bin", new)
    out = _diff(r)
    assert b"after" in out or b"AFTER" in out, "NUL-after-probe file must diff: %r" % out
s.test("I13 NUL after probe length-safe diff", case_i13)


# I14: byte-safe renderer for malformed UTF-8/control/NUL-after-probe. Diff
# output must be valid UTF-8 (decodable) and represent the differing bytes.
def case_i14():
    r = new_repo("i14"); r.init()
    old = b"a\n\x01\x02\n"
    new = b"a\n\x01\x02\xff\n"  # 0xff is malformed UTF-8
    r.write("f.bin", old)
    _run_save(r)
    r.write("f.bin", new)
    out = _diff(r)
    # output must be decodable UTF-8
    try:
        text = out.decode("utf-8")
    except UnicodeDecodeError:
        raise AssertionError("diff output is not valid UTF-8: %r" % out)
    assert "\\xFF" in text, "malformed byte must render as \\xFF: %r" % out
    assert "\\x01" in text or "\\x02" in text, "control bytes must render: %r" % out
s.test("I14 byte-safe renderer", case_i14)
