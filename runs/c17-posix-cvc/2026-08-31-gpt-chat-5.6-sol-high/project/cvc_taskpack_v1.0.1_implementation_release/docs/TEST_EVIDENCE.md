# Final Test and Release Evidence

Date of final validation: **2026-08-30**.

Environment details are retained in `docs/evidence/environment.txt`. The primary toolchain was GCC 14.2.0 on x86-64 Linux; Clang 17.0.0 was additionally used as an independent warning-strict compiler. Python is used only by black-box test automation.

## 1. Clean complete test run

Command:

```sh
make clean
make -j2 test
```

Result: **PASS** (`FINAL_TEST_RC=0`).

Summary from `docs/evidence/clean-test.log`:

```text
UNIT SUMMARY: 50/50 passed
mergebase_unit: PASS
ACCEPTANCE SUMMARY: 202/202 PASS, 0 FAIL, 0 MISSING, 0 SKIPPED
SUPPLEMENTAL SUMMARY: 2/2 passed
```

The acceptance runner's A01 case itself performs another `make clean` followed by a build of the production executable, core unit binary, merge-base test, and test-only fault injector. The remaining 201 mapped mandatory cases then run against the real production executable in temporary repositories.

The 202 mandatory IDs exactly cover the task-pack matrix, including J11b and L07b. Category counts are documented in `TRACEABILITY.md`.

Raw evidence: [`evidence/clean-test.log`](evidence/clean-test.log).

## 2. GCC warning-strict clean build

Command:

```sh
make clean
make CFLAGS='-std=c17 -O2 -g -Wall -Wextra -Wpedantic -Wshadow -Wconversion -Wsign-conversion -Wstrict-prototypes -Werror -D_POSIX_C_SOURCE=200809L' \
  -j2 all unit tests/mergebase_unit tests/libfaultio.so
```

Result: **PASS**, zero compiler warnings promoted to errors.

Raw evidence: [`evidence/gcc-werror-build.log`](evidence/gcc-werror-build.log).

## 3. GCC static analyzer

Production source was rebuilt with GCC `-fanalyzer` plus the normal strict warning family:

```sh
make clean
make CFLAGS='-std=c17 -O0 -g -Wall -Wextra -Wpedantic -Wshadow -Wconversion -Wsign-conversion -Wstrict-prototypes -fanalyzer -D_POSIX_C_SOURCE=200809L' -j2 all
```

Final result: **PASS with zero analyzer warnings**.

This pass found and led to correction of a latent buffer-formatting boundary before release: `cvc_buf_printf` now reserves space for the `vsnprintf` trailing NUL even for an empty formatted result. It also motivated explicit null/state guards and zero-count sort guards. The full acceptance suite was rerun after those fixes.

Raw evidence: [`evidence/gcc-fanalyzer.log`](evidence/gcc-fanalyzer.log).

## 4. ASan + UBSan

Command:

```sh
make sanitize
```

The Makefile rebuilds production/core test objects with:

```text
-fsanitize=address,undefined -fno-omit-frame-pointer
```

and runs the 50 core tests plus the dedicated merge-base graph test with leak detection enabled.

Result:

```text
UNIT SUMMARY: 50/50 passed
mergebase_unit: PASS
```

No AddressSanitizer, UndefinedBehaviorSanitizer, or leak report was emitted.

Raw evidence: [`evidence/asan-ubsan.log`](evidence/asan-ubsan.log).

## 5. Independent Clang warning-strict build

Command:

```sh
make clean
make CC=clang CFLAGS='-std=c17 -O2 -g -Wall -Wextra -Wpedantic -Wshadow -Wconversion -Wsign-conversion -Wstrict-prototypes -Werror -D_POSIX_C_SOURCE=200809L' \
  -j2 all unit tests/mergebase_unit
./tests/unit_core
./tests/mergebase_unit
```

Result: **PASS**; Clang builds production and both C test executables warning-free, unit tests are **50/50**, and merge-base test passes.

Raw evidence:

- [`evidence/clang-werror-build.log`](evidence/clang-werror-build.log)
- [`evidence/clang-unit.log`](evidence/clang-unit.log)
- [`evidence/clang-mergebase.log`](evidence/clang-mergebase.log)

## 6. Failure-injection and safety evidence

The mandatory O-category tests use `tests/faultio.c` only as a test harness via `LD_PRELOAD`; production has no test bypass. Injected failures cover initialization, object installation, ref update, worktree installation/rollback, and related durable-update paths. The mandatory result is **O01–O10 PASS**.

Two supplemental regressions run after all mandatory cases:

1. **structural symlink restore safety** — when HEAD structurally tracks `d/...` but working path `d` is locally replaced by a symlink to an outside directory, `restore d` replaces the symlink itself and leaves the outside target untouched;
2. **post-rename fsync rollback** — if a target file has already been installed by `rename` and the following parent-directory `fsync` is then injected to fail, the command returns failure, removes the newly installed path, restores prior tracked state, and leaves HEAD unchanged.

Result: **2/2 PASS**.

## 7. Prohibited-implementation and dependency audit

The production `src/` and `include/` trees were searched for delegated/subprocess calls including `system`, `popen`, `fork`/`exec`, `posix_spawn`, POSIX `glob`/`fnmatch`, `ftw`/`nftw`/`fts`, `wordexp`, `iconv`, external hash APIs, and dynamic-loader APIs. No production call matched.

A separate production token scan found no implementation reference to Git, Mercurial, Fossil, SVN, rsync, sha256sum, diff3, patch, or jq. A TODO/FIXME/placeholder/mock scan found no production placeholder marker.

The only production `getenv` occurrence is the specification-permitted `CVC_TEST_TIMESTAMP` timestamp hook.

Dynamic dependency inspection of the normal release-equivalent binary reports only `libc.so.6` and the ELF loader/vDSO. Undefined symbols are ordinary ISO C / permitted POSIX filesystem and locking calls; no third-party runtime implementation library appears.

Raw evidence: [`evidence/prohibited-audit.log`](evidence/prohibited-audit.log).

## 8. Canonical object evidence

Core tests independently check the fixed IDs:

```text
blob 0\0                 473a0f4c3be8a93681a267e3b1e9a7dcda1185436fe141f7749120a303721813
blob 3\0abc               c1cf6e465077930e88dc5136641d402f72a229ddd996f627d60e9639eaba35a6
canonical empty tree     37b344f390f440a6a43040c9b0da9937d8f0d9d2b4db80cd1e2385054835c50f
canonical root commit    b76903cf9661046c99f6f4d4e9ceda05cef2607b47bd9b2f9396ea67ad1e72ab
```

All four passed in the final clean run.

## 9. Release Gate conclusion

After the final clean run, source/dependency audit, manual checklist review, analyzer, sanitizer, and cross-compiler build, there are **no known mandatory failures** and no skipped mandatory cases. Release Gates **R1 through R9 are PASS**. See `TRACEABILITY.md` for the evidence mapping and `MANUAL_CHECKLIST.md` for the task-pack manual checklist review.
