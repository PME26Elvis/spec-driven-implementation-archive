# testlib.py - Shared helpers for the CVC automated acceptance suite.
# Runs against the real production cvc.exe. Tests create isolated temp
# repositories under the D: drive (never C:) and clean them up.

import os, sys, subprocess, tempfile, shutil, hashlib, struct, re

CVC = os.environ.get("CVC_EXE", r"D:\0831-cvc-workbuddy\.cvc_build_tmp\bin\cvc.exe")
# Workroot must NOT have any CVC repository as an ancestor, or "not a
# repository" tests would falsely discover an outer repo. D:\ root is clean.
WORKROOT = os.environ.get("CVC_TEST_WORKROOT", r"D:\cvctest_auto")

# ---- minimal test framework -------------------------------------------------
class Suite:
    def __init__(self, name):
        self.name = name
        self.cases = []
    def test(self, desc, fn):
        self.cases.append((desc, fn))
    def run(self):
        passed = failed = skipped = 0
        failures = []
        for desc, fn in self.cases:
            try:
                fn()
                passed += 1
                sys.stdout.write("  PASS  %s\n" % desc)
            except Skip as e:
                skipped += 1
                sys.stdout.write("  SKIP  %s (%s)\n" % (desc, e))
            except AssertionError as e:
                failed += 1
                failures.append((desc, str(e)))
                sys.stdout.write("  FAIL  %s :: %s\n" % (desc, e))
            except Exception as e:
                failed += 1
                failures.append((desc, "EXC: %r" % (e,)))
                sys.stdout.write("  FAIL  %s :: %r\n" % (desc, e))
        return passed, failed, skipped, failures

class Skip(Exception):
    pass

ALL_SUITES = []

def suite(name):
    s = Suite(name)
    ALL_SUITES.append(s)
    return s

def fs_is_case_sensitive(d):
    """Probe whether a directory is case-sensitive by trying to create two
    names differing only by ASCII case. Returns True if they coexist with
    distinct content (i.e. the filesystem keeps them separate)."""
    probe = os.path.join(d, "__cs_probe__")
    os.makedirs(probe, exist_ok=True)
    a = os.path.join(probe, "Probe")
    b = os.path.join(probe, "probe")
    try:
        with open(a, "w") as f:
            f.write("A")
        with open(b, "w") as f:
            f.write("B")
        return (os.path.exists(a) and os.path.exists(b)
                and os.path.getsize(a) == 1 and os.path.getsize(b) == 1
                and open(a).read() == "A" and open(b).read() == "B")
    except OSError:
        return False
    finally:
        shutil.rmtree(probe, ignore_errors=True)

def require_symlink_support():
    # Windows symlink creation needs admin or Developer Mode. Probe it.
    t = os.path.join(WORKROOT, "symlink_probe")
    os.makedirs(t, exist_ok=True)
    target = os.path.join(t, "real.txt")
    link = os.path.join(t, "link.txt")
    with open(target, "w") as f:
        f.write("x")
    try:
        if os.path.exists(link):
            os.remove(link)
        os.symlink("real.txt", link, target_is_directory=False)
        ok = os.path.islink(link)
    except OSError:
        ok = False
    finally:
        shutil.rmtree(t, ignore_errors=True)
    return ok

# ---- repo / command helpers -------------------------------------------------
class Repo:
    def __init__(self, path):
        self.path = path
    def run(self, *args, cwd=None, env=None, input_bytes=None, expect_rc=0,
            timeout=60):
        e = dict(os.environ)
        if env:
            e.update(env)
        p = subprocess.run([CVC] + list(args), cwd=cwd or self.path,
                           env=e, capture_output=True, timeout=timeout,
                           input=input_bytes)
        return p
    def write(self, rel, data, mode="wb"):
        p = os.path.join(self.path, rel.replace("/", os.sep))
        os.makedirs(os.path.dirname(p), exist_ok=True)
        if isinstance(data, str):
            data = data.encode("utf-8")
        with open(p, mode) as f:
            f.write(data)
        return p
    def read(self, rel):
        p = os.path.join(self.path, rel.replace("/", os.sep))
        with open(p, "rb") as f:
            return f.read()
    def exists(self, rel):
        return os.path.exists(os.path.join(self.path, rel.replace("/", os.sep)))
    def unlink(self, rel):
        os.unlink(os.path.join(self.path, rel.replace("/", os.sep)))
    def init(self, **kw):
        return self.run("init", **kw)

def new_repo(name):
    d = os.path.join(WORKROOT, name)
    if os.path.exists(d):
        shutil.rmtree(d, ignore_errors=True)
    os.makedirs(d)
    return Repo(d)

def cleanup_workroot():
    # Remove each test repo individually (rather than one giant rmtree of the
    # whole workroot) so that large accumulated trees don't trip bulk-delete
    # guards and so failures in one cleanup don't cascade.
    if os.path.exists(WORKROOT):
        for name in os.listdir(WORKROOT):
            p = os.path.join(WORKROOT, name)
            try:
                if os.path.isdir(p) and not os.path.islink(p):
                    shutil.rmtree(p, ignore_errors=True)
                else:
                    os.unlink(p)
            except OSError:
                pass
    else:
        os.makedirs(WORKROOT, exist_ok=True)

# ---- object helpers for independent verification ----------------------------
def obj_envelope(typ, payload):
    return ("%s %d\x00" % (typ, len(payload))).encode() + payload

def oid(b):
    return hashlib.sha256(b).hexdigest()

def empty_tree_bytes():
    return obj_envelope("tree", struct.pack(">I", 0))

EMPTY_TREE_ID = "37b344f390f440a6a43040c9b0da9937d8f0d9d2b4db80cd1e2385054835c50f"

def obj_path(root, oidhex):
    return os.path.join(root, ".cvc", "objects", oidhex[:2], oidhex[2:])

def read_loose_object(root, oidhex):
    """Return (type, payload) by reading and validating a loose object."""
    with open(obj_path(root, oidhex), "rb") as f:
        raw = f.read()
    nul = raw.index(b"\x00")
    head = raw[:nul]
    m = re.match(br"^([a-z]+) ([0-9]+)$", head)
    if not m:
        raise AssertionError("bad envelope header: %r" % head)
    typ = m.group(1).decode()
    n = int(m.group(2))
    payload = raw[nul+1:nul+1+n]
    if len(payload) != n or len(raw) != nul+1+n:
        raise AssertionError("envelope length mismatch")
    if oid(raw) != oidhex:
        raise AssertionError("object hash mismatch")
    return typ, payload

def saved_commit_id(r):
    """Return the hex id of the most recent commit (assumes a commit exists)."""
    p = r.run("log")
    m = re.search(rb"commit ([0-9a-f]{64})", p.stdout)
    assert m, "expected a commit in log: %r" % p.stdout
    return m.group(1).decode()
