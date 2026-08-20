#!/usr/bin/env python3
import json, subprocess, sys, os

root = "D:/0814"
proj = root + "/pinball_sandbox_v1.0.0"
spec = root + "/pinball_benchmark_spec_windows_v1.0.0/acceptance"
pc = proj + "/build/pcheck.exe"

manifest = json.load(open(spec + "/parser_corpus_manifest.json", encoding="utf-8"))

pass_n = 0
fail_n = 0
fails = []

def run(fn):
    p = subprocess.run([pc, fn], capture_output=True, text=True)
    line = (p.stdout or "").splitlines()
    if line:
        return line[0].strip(), p.stdout
    # crash: try stderr
    return ("<CRASH exit=%d>" % p.returncode), (p.stderr or "")

for c in manifest["cases"]:
    fn = spec + "/" + c["file"]
    code, out = run(fn)
    exp = c["expected_primary_code"]
    ok = (code == exp)
    if ok:
        pass_n += 1
    else:
        fail_n += 1
        fails.append((c["id"], exp, code, out[:300]))
    print(("%-28s exp=%-26s got=%-26s %s" % (c["id"], exp, code, "OK" if ok else "MISMATCH")))

for c in manifest["valid_cases"]:
    fn = spec + "/" + c["file"]
    code, out = run(fn)
    ok = (code == "PBT_OK")
    if ok:
        pass_n += 1
    else:
        fail_n += 1
        fails.append((c["file"], "PBT_OK", code, out[:300]))
    print(("%-28s exp=%-26s got=%-26s %s" % (c["file"].split('/')[-1], "PBT_OK", code, "OK" if ok else "MISMATCH")))

print("\n=== SUMMARY: %d passed, %d failed ===" % (pass_n, fail_n))
if fails:
    print("\nFAILURES:")
    for fid, exp, got, out in fails:
        print("\n--- %s ---\n  exp=%s got=%s\n%s" % (fid, exp, got, out))
    sys.exit(1)
sys.exit(0)
