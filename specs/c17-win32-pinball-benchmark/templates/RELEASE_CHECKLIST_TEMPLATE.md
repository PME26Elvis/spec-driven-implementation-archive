# Release Checklist

Release candidate identifier: `<fill>`  
Build identifier/commit: `<fill>`  
Task package version: `1.0.0`  
Application version: `<fill>`  
Date: `<fill>`

Allowed statuses: `PASS`, `FAIL`, `BLOCKED`, `NOT RUN`.

| Gate | Status | Evidence / report reference | Notes |
|---|---|---|---|
| Build | NOT RUN | | |
| Dependency | NOT RUN | | |
| Main UI | NOT RUN | | |
| Editor | NOT RUN | | |
| Advanced Editor | NOT RUN | | |
| Persistence | NOT RUN | | |
| Physics Core | NOT RUN | | |
| Determinism | NOT RUN | | |
| Gameplay | NOT RUN | | |
| Pinball Mechanisms and Tilt | NOT RUN | | |
| Replay | NOT RUN | | |
| Physics Inspector | NOT RUN | | |
| Desktop Interaction / HiDPI | NOT RUN | | |
| Reliability / Recovery | NOT RUN | | |
| Diagnostics / Trace | NOT RUN | | |
| Headless | NOT RUN | | |
| Engineering Utilities | NOT RUN | | |
| Automated Tests | NOT RUN | | |
| Visual Evidence | NOT RUN | | |
| Stress | NOT RUN | | |
| Performance / Resource | NOT RUN | | |
| Canonical E2E | NOT RUN | | |
| Release Evidence | NOT RUN | | |
| Error Handling | NOT RUN | | |
| Anti-placeholder / Integrity | NOT RUN | | |

## Mandatory integrity declaration

- [ ] No prohibited dependency or substitute implementation is knowingly present.
- [ ] Acceptance fixtures/results are not special-cased in production code.
- [ ] GUI and headless simulation use the same production physics/event logic.
- [ ] Trace/Inspector/Statistics values originate from production runtime state.
- [ ] Visual evidence was captured from this exact release candidate.
- [ ] Autosave/recovery never silently overwrites formal user data.
- [ ] `RELEASE_EVIDENCE.json` covers every stable requirement ID.
- [ ] `releasecheck` passes.
- [ ] All known mandatory failures are represented as FAIL/BLOCKED/NOT RUN rather than hidden.

## Release conclusion

Overall status: `<PASS only if every mandatory Release Gate passes>`

Unresolved mandatory items:

- `<none, or list each item>`


## Windows Platform Binding Gate

- [ ] PASS / [ ] FAIL
- [ ] Real Win32 top-level window
- [ ] no prohibited native controls/dialog substitution
- [ ] software-owned framebuffer/rendering
- [ ] Per-Monitor DPI v2 behavior verified
- [ ] Unicode path/clipboard/IME verified
- [ ] headless creates no HWND
- [ ] USER/GDI/HANDLE resource cycle stable
- [ ] platform import/source audit complete
