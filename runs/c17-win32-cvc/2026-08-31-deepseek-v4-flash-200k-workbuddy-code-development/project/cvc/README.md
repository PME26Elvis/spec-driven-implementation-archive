# CVC — Content-Addressed Version Control (Windows, C17)

CVC is a native Windows command-line version control system written entirely in
C17. It stores content-addressed, immutable snapshots in a git-like object
model (blob / tree / commit) using the **fixed v1 canonical serialization**
required by the CVC Windows task pack. It has **no third-party dependencies**:
JSON parsing, UTF-8 validation, SHA-256, glob matching, Myers diff, three-way
merge, history traversal, and repository verification are all hand-written in
project C code.

The executable is `cvc.exe`, a 64-bit native Win32 application. It does **not**
require Cygwin, MSYS, WSL, or any shell to run; it does not shell out to
`git`, `diff`, `patch`, or any external tool for required behavior.

---

## Build

Requirements:

- MinGW-w64 GCC (C17) toolchain. This project was built against
  `D:\0831-cvc-workbuddy\toolchain\mingw64`.
- `bash` (Git Bash) and `cygpath` for the build script.

Build from a clean checkout:

```bash
cd cvc
bash tools/build.sh all
```

This compiles every `src/*.c` and links `cvc.exe`. Output is written to
`D:\0831-cvc-workbuddy\.cvc_build_tmp\bin\cvc.exe` (kept on `D:` to conserve
`C:` space, per the environment constraint). Build flags: `-std=c17 -O2
-Wall -Wextra`.

`tools/build.sh` accepts:

```bash
bash tools/build.sh all      # compile all objects and link (default)
bash tools/build.sh exe      # relink only
bash tools/build.sh clean    # remove object/bin output
```

There are **no hard-coded absolute paths in any source file**; the only paths
are in the build script's output directory and toolchain location, which are
build-environment choices (not source). The build requires no manual patching
and no generated source beyond what is checked in.

### Source layout

| Path        | Contents                                                        |
|-------------|-----------------------------------------------------------------|
| `src/*.c`   | 16 translation units (see below)                                |
| `include/*` | Headers (`objects.h`, `repo.h`, `merge.h`, `win32.h`, …)        |
| `tests/run` | Python acceptance suite (uses the real `cvc.exe`)               |
| `tools/`    | `build.sh`, `build.ps1`, `vcvars_wrapper.cmd`                   |

Source modules:

```
util.c      sha256.c   utf8.c    json.c     glob.c    diff.c
win32.c     repo.c     objects.c scan.c     snapshot.c
materialize.c merge.c  verify.c  cli.c      main.c
```

---

## Running tests

The acceptance suite is a set of self-contained Python scripts that drive the
real `cvc.exe` and verify behavior, object bytes, hashes, error codes, and
repository layout.

```bash
# From the cvc directory, with CVC_EXE pointing at the built binary:
CVC_EXE="D:\0831-cvc-workbuddy\.cvc_build_tmp\bin\cvc.exe" \
  C:/Users/BATLAB/.workbuddy/binaries/python/versions/3.13.12/python.exe \
  tests/run/run_tests.py

# Filter to one category:
... python.exe tests/run/run_tests.py "Merge"
# List all cases:
... python.exe tests/run/run_tests.py --list
```

The runner returns non-zero if any mandatory test fails. Test repositories are
created under `D:\cvctest_auto` (never `C:`). See
`docs/TEST_EVIDENCE.md` for the latest clean-run results and the R1–R9 release
gate status.

---

## Usage

Run `cvc` from inside a repository, or from any subdirectory of one (CVC
discovers the repository root). Initialize with:

```text
cvc init
```

Common commands:

| Command                                      | Description                                                        |
|----------------------------------------------|--------------------------------------------------------------------|
| `cvc init`                                   | Initialize a repository in the current directory                  |
| `cvc status`                                 | Show working-tree changes relative to `HEAD`                      |
| `cvc save -m <message>`                      | Record a new snapshot of tracked files                            |
| `cvc log [--max-count=<N>]`                  | Show commit history (first-parent)                                |
| `cvc diff [<revision>]`                      | Show working-tree changes against a revision                      |
| `cvc branch`                                 | List branches                                                     |
| `cvc branch create <name>`                   | Create a branch at current commit                                 |
| `cvc branch delete <name>`                   | Delete a branch                                                   |
| `cvc switch <branch>`                        | Switch to a branch                                                |
| `cvc restore <path> --from <rev>`            | Restore a tracked path from a revision                            |
| `cvc rollback <rev> -m <msg>`                | Create a new commit reverting to a revision's tree                |
| `cvc merge <branch> [-m <msg>]`              | Merge a branch (fast-forward or divergent three-way)              |
| `cvc resolve <path>`                         | Mark a conflict path resolved                                      |
| `cvc merge --continue` / `cvc merge --abort` | Continue or abort an in-progress merge                            |
| `cvc verify`                                 | Check repository integrity                                         |

Exit codes: `0` success; `1` generic error; `2` usage error; `5` repository
busy (a conflicting `LockFileEx` repository lock is held).

---

## Repository layout

```
.cvc/
  config.json        # schema-validated JSON configuration
  HEAD               # "ref: refs/heads/main"
  lock               # persistent zero-byte file; LockFileEx byte-range lock
  objects/xx/…       # content-addressed loose objects (fan-out by first 2 hex)
  refs/heads/…       # branch refs (65-byte hex + "\n")
  state/             # merge state (only during an in-progress/finalizing merge)
```

See `docs/IMPLEMENTATION.md` for the exact canonical object serialization,
lock protocol, and atomic publication rules.
