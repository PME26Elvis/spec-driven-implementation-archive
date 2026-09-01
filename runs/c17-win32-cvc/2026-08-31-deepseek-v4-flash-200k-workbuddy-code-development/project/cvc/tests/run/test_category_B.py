# Category B - Initialization
from testlib import *

s = suite("B-Initialization")

# B01: init creates repository metadata
def case_b01():
    r = new_repo("b01")
    p = r.init()
    assert p.returncode == 0, p.stderr
    for sub in [".cvc", ".cvc/refs/heads", ".cvc/objects", ".cvc/state"]:
        assert os.path.isdir(os.path.join(r.path, sub)), "missing %s" % sub
    assert os.path.isfile(os.path.join(r.path, ".cvc/HEAD"))
    assert os.path.isfile(os.path.join(r.path, ".cvc/lock"))
    assert os.path.isfile(os.path.join(r.path, ".cvc/config.json"))
s.test("B01 init creates repo metadata", case_b01)

# B02: second init refuses overwrite
def case_b02():
    r = new_repo("b02")
    r.init()
    p = r.init()
    assert p.returncode != 0, "second init should refuse"
s.test("B02 second init refuses overwrite", case_b02)

# B03: default branch is main
def case_b03():
    r = new_repo("b03")
    r.init()
    with open(os.path.join(r.path, ".cvc", "HEAD"), "rb") as f:
        assert f.read() == b"ref: refs/heads/main\n", "HEAD wrong"
    assert os.path.isfile(os.path.join(r.path, ".cvc", "refs", "heads", "main"))
    assert os.path.getsize(os.path.join(r.path, ".cvc", "refs", "heads", "main")) == 0
s.test("B03 default branch is main", case_b03)

# B04: empty repo status/log do not crash
def case_b04():
    r = new_repo("b04")
    r.init()
    p = r.run("status")
    assert p.returncode == 0, p.stderr
    p = r.run("log")
    assert p.returncode == 0, p.stderr
s.test("B04 empty repo status/log no crash", case_b04)

# B05: save on empty unborn repo creates no commit
def case_b05():
    r = new_repo("b05")
    r.init()
    p = r.run("save", "-m", "x")
    assert p.returncode == 0, p.stderr
    # still unborn
    with open(os.path.join(r.path, ".cvc", "refs", "heads", "main"), "rb") as f:
        assert f.read() == b"", "unborn branch should be zero-length"
s.test("B05 save on unborn repo creates no commit", case_b05)

# B06: exact HEAD/unborn-main layout + zero-length lock
def case_b06():
    r = new_repo("b06")
    r.init()
    with open(os.path.join(r.path, ".cvc", "HEAD"), "rb") as f:
        assert f.read() == b"ref: refs/heads/main\n"
    with open(os.path.join(r.path, ".cvc", "lock"), "rb") as f:
        assert f.read() == b"", "lock should be zero-length"
    assert os.path.getsize(os.path.join(r.path, ".cvc", "lock")) == 0
s.test("B06 exact v1 HEAD layout and zero lock", case_b06)

# B07: partial init failure leaves no false-valid repo
def case_b07():
    # init must fail rather than overwrite a preexisting entry aliasing .cvc
    r = new_repo("b07")
    os.makedirs(os.path.join(r.path, ".cvc"))
    with open(os.path.join(r.path, ".cvc", "not_a_repo"), "w") as f:
        f.write("x")
    p = r.init()
    assert p.returncode != 0, "init must not overwrite preexisting .cvc"
    # the preexisting entry must remain
    assert os.path.exists(os.path.join(r.path, ".cvc", "not_a_repo"))
s.test("B07 partial init failure no false-valid repo", case_b07)
