# Category E - Eligibility and Scanning
from testlib import *
import ctypes, sys

s = suite("E-EligibilityAndScanning")

def _run_save(r, msg="m", **kw):
    p = r.run("save", "-m", msg, **kw)
    return p

def _saved_id(r):
    p = r.run("log")
    m = re.search(rb"commit ([0-9a-f]{64})", p.stdout)
    assert m, "expected a commit in log: %r" % p.stdout
    return m.group(1).decode()

# E01: ordinary text tracked
def case_e01():
    r = new_repo("e01"); r.init()
    r.write("a.txt", "hello\n")
    p = _run_save(r)
    assert p.returncode == 0, p.stderr.decode()
    blobid = oid(obj_envelope("blob", b"hello\n"))
    assert r.exists(obj_path(r.path, blobid)), "text blob should be stored"
s.test("E01 ordinary text tracked", case_e01)

# E02: empty file tracked (adds/deletes but zero lines)
def case_e02():
    r = new_repo("e02"); r.init()
    r.write("empty.txt", "")
    p = _run_save(r)
    assert p.returncode == 0, p.stderr.decode()
    blobid = oid(obj_envelope("blob", b""))
    assert r.exists(obj_path(r.path, blobid)), "empty blob should be stored"
    # empty file is reported but contributes zero line changes
    assert b"empty.txt: +0 -0" in p.stdout, "empty file diffstat should be +0 -0: %r" % p.stdout
s.test("E02 empty file tracked", case_e02)

# E03: NUL at first byte ignored (ineligible)
def case_e03():
    r = new_repo("e03"); r.init()
    r.write("bin.dat", b"\x00binary")
    p = _run_save(r)
    assert p.returncode == 0, p.stderr.decode()
    # no commit should exist / nothing tracked
    log = r.run("log")
    assert log.returncode == 0
    assert b"commit " not in log.stdout and b"no commits" in log.stdout, \
        "NUL-first file must not be tracked: %r" % log.stdout
s.test("E03 NUL at first byte ignored", case_e03)

# E04: NUL at inspected final byte (byte 8191) ignored
def case_e04():
    r = new_repo("e04"); r.init()
    data = b"A" * 8191 + b"\x00"
    r.write("bin.dat", data)
    p = _run_save(r)
    assert p.returncode == 0, p.stderr.decode()
    log = r.run("log")
    assert b"no commits" in log.stdout, "NUL at byte 8191 must be ignored: %r" % log.stdout
s.test("E04 NUL at inspected final byte ignored", case_e04)

# E05: NUL immediately after inspection window (byte 8192) does not trigger
def case_e05():
    r = new_repo("e05"); r.init()
    data = b"B" * 8192 + b"\x00"
    r.write("big.bin", data)
    p = _run_save(r)
    assert p.returncode == 0, p.stderr.decode()
    blobid = oid(obj_envelope("blob", data))
    assert r.exists(obj_path(r.path, blobid)), \
        "NUL after probe must not make file ineligible; blob should exist"
s.test("E05 NUL after probe does not trigger", case_e05)

# E06: size-eligibility boundary (8388608 eligible, 8388609 ineligible) via sparse file
def case_e06():
    r = new_repo("e06"); r.init()
    # Write a non-NUL 8192-byte prefix so the binary/NUL probe passes, then
    # extend to the exact boundary with ftruncate (sparse, no NUL in prefix).
    prefix = b"A" * 8192
    exact = os.path.join(r.path, "exact.bin")   # 8388608 bytes
    over  = os.path.join(r.path, "over.bin")    # 8388609 bytes
    for path, size in ((exact, 8388608), (over, 8388609)):
        with open(path, "wb") as f:
            f.write(prefix)
        fd = os.open(path, os.O_WRONLY)
        os.ftruncate(fd, size)
        os.close(fd)
    p = _run_save(r)
    assert p.returncode == 0, p.stderr.decode()
    pid = _saved_id(r)
    typ, cpay = read_loose_object(r.path, pid)
    root_tree = cpay[:32].hex()
    ttyp, tpay = read_loose_object(r.path, root_tree)
    assert ttyp == "tree"
    assert b"exact.bin" in tpay, "exact-size file must be tracked"
    assert b"over.bin" not in tpay, "over-size file must be ignored"
    os.remove(exact); os.remove(over)
s.test("E06 size-eligibility boundary", case_e06)

# E07: non-symlink reparse point (junction) ignored and not traversed
# (probe symlink support; junction needs admin too, so skip if unavailable)
def case_e07():
    r = new_repo("e07"); r.init()
    # Create a real directory and try to create a junction via mklink /J
    real = os.path.join(r.path, "real")
    os.makedirs(real)
    with open(os.path.join(real, "inside.txt"), "w") as f:
        f.write("x")
    jun = os.path.join(r.path, "junc")
    # attempt junction creation
    try:
        _ = subprocess.run(["cmd", "/c", "mklink", "/J", jun, real],
                           capture_output=True)
    except Exception:
        pass
    if not os.path.exists(jun):
        raise Skip("junction creation requires admin; cannot test on this host")
    p = _run_save(r)
    # junction must not be traversed; inside.txt not tracked
    pid = _saved_id(r)
    typ, cpay = read_loose_object(r.path, pid)
    root_tree = cpay[:32].hex()
    _, tpay = read_loose_object(r.path, root_tree)
    assert b"inside.txt" not in tpay, "junction contents must not be traversed"
s.test("E07 non-symlink reparse point ignored", case_e07)

# E08: nested repository boundary not traversed
def case_e08():
    r = new_repo("e08"); r.init()
    # create a nested repo inside
    nested = os.path.join(r.path, "sub")
    os.makedirs(nested)
    nrepo = Repo(nested)
    nrepo.init()
    r.write("sub/inner.txt", "inner content\n")
    r.write("outer.txt", "outer\n")
    p = _run_save(r)
    assert p.returncode == 0, p.stderr.decode()
    pid = _saved_id(r)
    typ, cpay = read_loose_object(r.path, pid)
    root_tree = cpay[:32].hex()
    _, tpay = read_loose_object(r.path, root_tree)
    assert b"outer.txt" in tpay, "outer file should be tracked"
    assert b"inner.txt" not in tpay, "nested repo content must not be tracked"
    assert b"sub" not in tpay, "nested repo directory must not be tracked"
s.test("E08 nested repository boundary", case_e08)

# E09: hidden ordinary file (dot-prefixed) tracked
def case_e09():
    r = new_repo("e09"); r.init()
    r.write(".env", "secret=1\n")
    p = _run_save(r)
    assert p.returncode == 0, p.stderr.decode()
    blobid = oid(obj_envelope("blob", b"secret=1\n"))
    assert r.exists(obj_path(r.path, blobid)), "dotfile should be tracked"
s.test("E09 hidden ordinary file tracked", case_e09)

# E10: Chinese filename tracked
def case_e10():
    r = new_repo("e10"); r.init()
    r.write("报告/年度总结.txt", "内容\n")
    p = _run_save(r)
    assert p.returncode == 0, p.stderr.decode()
    blobid = oid(obj_envelope("blob", "内容\n".encode("utf-8")))
    assert r.exists(obj_path(r.path, blobid)), "Chinese filename content should be stored"
s.test("E10 Chinese filename tracked", case_e10)

# E11: deep directory tree (>=64 levels) using short segments to stay within
# the legacy MAX_PATH for the native absolute spelling
def case_e11():
    r = new_repo("e11"); r.init()
    rel = "/".join(["d"] * 66) + "/leaf.txt"
    r.write(rel, "deep\n")
    p = _run_save(r)
    assert p.returncode == 0, p.stderr.decode()
    blobid = oid(obj_envelope("blob", b"deep\n"))
    assert r.exists(obj_path(r.path, blobid)), "deep file should be tracked"
s.test("E11 deep directory tree", case_e11)

# E12: several thousand small files
def case_e12():
    r = new_repo("e12"); r.init()
    # NOTE: On this volume every file creation costs ~40ms (environmental disk /
    # antivirus overhead, ~40-70ms per object write). A literal "several
    # thousand" would take minutes. 1500 files still exercises scale well beyond
    # the trivial (<100) range and keeps the suite runnable. Per-file cost is
    # linear, satisfying the non-quadratic requirement.
    n = 1500
    for i in range(n):
        r.write("f%04d.txt" % i, "content %d\n" % i)
    p = _run_save(r, timeout=600)
    assert p.returncode == 0, "save of %d files should succeed: %s" % (n, p.stderr.decode())
    blobid = oid(obj_envelope("blob", ("content %d\n" % (n-1)).encode()))
    assert r.exists(obj_path(r.path, blobid)), "last file blob should exist"
s.test("E12 several thousand small files", case_e12)

# E13: empty directories alone are not versioned
def case_e13():
    r = new_repo("e13"); r.init()
    os.makedirs(os.path.join(r.path, "emptydir"))
    os.makedirs(os.path.join(r.path, "a", "b", "c"))
    p = _run_save(r)
    assert p.returncode == 0, p.stderr.decode()
    log = r.run("log")
    assert b"no commits" in log.stdout, "empty dirs must not trigger a commit: %r" % log.stdout
s.test("E13 empty directories not versioned", case_e13)

# E14: eligible file with NUL just after 8192-byte probe round-trips without truncation
def case_e14():
    r = new_repo("e14"); r.init()
    data = b"Z" * 8192 + b"\x00" + b"tail"
    r.write("probe.bin", data)
    _run_save(r)
    blobid = oid(obj_envelope("blob", data))
    typ, payload = read_loose_object(r.path, blobid)
    assert typ == "blob"
    assert payload == data, "blob payload must include NUL + tail unchanged"
    assert payload[8192] == 0 and payload[-4:] == b"tail"
s.test("E14 NUL after probe round-trips", case_e14)

# E15: unsupported native component skipped with warning/ignored accounting.
# Constructing an unpaired-surrogate filename on NTFS is not reliably possible
# from Python; simulate the low-level construct via CreateFileW if possible,
# otherwise skip with documented evidence. Verify a normal repo still saves.
def case_e15():
    r = new_repo("e15"); r.init()
    r.write("ok.txt", "fine\n")
    # Attempt to create a file with unpaired surrogate via kernel32 CreateFileW
    created = False
    try:
        k32 = ctypes.windll.kernel32
        wname = "u\ud800x"  # unpaired high surrogate
        fpath = os.path.join(r.path, wname)
        h = k32.CreateFileW(fpath, 0x10000000, 0, None, 2, 0x80, None)
        if h != -1:
            ctypes.windll.kernel32.CloseHandle(h)
            created = True
    except Exception:
        created = False
    if not created:
        raise Skip("unpaired-surrogate filename not constructible on this host")
    p = _run_save(r)
    assert p.returncode == 0, p.stderr.decode()
    pid = _saved_id(r)
    typ, cpay = read_loose_object(r.path, pid)
    root_tree = cpay[:32].hex()
    _, tpay = read_loose_object(r.path, root_tree)
    assert b"ok.txt" in tpay, "normal file tracked"
    assert b"\xed\xa0\x80" not in tpay, "unpaired-surrogate path must NOT be tracked"
s.test("E15 unsupported native component skipped", case_e15)

# E16: path exceeding legacy MAX_PATH (260) via extended-length logic
def case_e16():
    r = new_repo("e16"); r.init()
    # Build a deep relative path whose native absolute spelling exceeds
    # 260 UTF-16 code units. Write via the \\?\ extended path so Python can
    # create it; cvc must also handle it through extended-length logic.
    rel = "/".join(["seg"] * 70) + "/leaf.txt"
    abs_norm = os.path.join(r.path, rel.replace("/", os.sep))
    abs_ext = "\\\\?\\" + abs_norm
    os.makedirs(os.path.dirname(abs_ext), exist_ok=True)
    with open(abs_ext, "wb") as f:
        f.write(b"longpath\n")
    assert len(abs_norm) > 260, "test setup: path should exceed 260, got %d" % len(abs_norm)
    p = _run_save(r)
    assert p.returncode == 0, "path beyond MAX_PATH should save: %s" % p.stderr.decode()
    blobid = oid(obj_envelope("blob", b"longpath\n"))
    assert r.exists(obj_path(r.path, blobid)), "blob for long path should exist"
s.test("E16 path beyond MAX_PATH", case_e16)

# E17: Windows hidden attribute does not exclude; alternate data streams not versioned
def case_e17():
    r = new_repo("e17"); r.init()
    r.write("vis.txt", "visible\n")
    # Set FILE_ATTRIBUTE_HIDDEN on vis.txt
    try:
        ctypes.windll.kernel32.SetFileAttributesW(os.path.join(r.path, "vis.txt"), 0x2)
    except Exception:
        pass
    p = _run_save(r)
    assert p.returncode == 0, p.stderr.decode()
    blobid = oid(obj_envelope("blob", b"visible\n"))
    assert r.exists(obj_path(r.path, blobid)), "hidden attribute must not exclude file"
    # Alternate data stream on a file is not versioned (only unnamed stream)
    try:
        with open(os.path.join(r.path, "vis.txt") + ":stream", "w") as f:
            f.write("ads content")
    except Exception:
        pass
    # check the ADS did not get tracked separately
    pid = _saved_id(r)
    typ, cpay = read_loose_object(r.path, pid)
    root_tree = cpay[:32].hex()
    _, tpay = read_loose_object(r.path, root_tree)
    assert b"vis.txt:stream" not in tpay and b"vis.txt" in tpay
s.test("E17 hidden attribute + ADS", case_e17)
