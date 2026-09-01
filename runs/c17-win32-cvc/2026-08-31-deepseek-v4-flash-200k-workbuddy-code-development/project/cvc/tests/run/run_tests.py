# run_tests.py - CVC automated acceptance test runner.
# Usage: python run_tests.py [category_filter] [--list]
#   category_filter: substring matched against suite name, e.g. "A-" or "Merge"
# Returns nonzero exit if any mandatory test fails.
import sys, os, glob, importlib.util

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from testlib import ALL_SUITES, cleanup_workroot, CVC

def main():
    argv = [a for a in sys.argv[1:]]
    do_list = "--list" in argv
    argv = [a for a in argv if a != "--list"]
    filt = argv[0] if argv else None

    # load all test_category_*.py modules
    here = os.path.dirname(os.path.abspath(__file__))
    for mod in sorted(glob.glob(os.path.join(here, "test_category_*.py"))):
        name = os.path.splitext(os.path.basename(mod))[0]
        spec = importlib.util.spec_from_file_location(name, mod)
        m = importlib.util.module_from_spec(spec)
        spec.loader.exec_module(m)

    if do_list:
        for s in ALL_SUITES:
            for desc, _ in s.cases:
                print("%s: %s" % (s.name, desc))
        return 0

    if not os.path.isfile(CVC):
        print("FATAL: cvc.exe not found at %s (set CVC_EXE)" % CVC, file=sys.stderr)
        return 2

    cleanup_workroot()

    total = passed = failed = skipped = 0
    all_failures = []
    for s in ALL_SUITES:
        if filt and filt.lower() not in s.name.lower():
            continue
        print("\n=== Suite %s ===" % s.name)
        p, f, sk, fails = s.run()
        total += p + f + sk
        passed += p; failed += f; skipped += sk
        all_failures.extend(fails)

    print("\n" + "=" * 50)
    print("TOTAL  : %d" % total)
    print("PASSED : %d" % passed)
    print("FAILED : %d" % failed)
    print("SKIPPED: %d" % skipped)
    if all_failures:
        print("\nFAILURES:")
        for d, e in all_failures:
            print("  [%s] %s" % (d, e))
    print("=" * 50)
    return 1 if failed > 0 else 0

if __name__ == "__main__":
    sys.exit(main())
