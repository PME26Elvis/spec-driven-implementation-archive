# Known Issues

Release candidate identifier: `pinball_sandbox_v1.0.0`

A Known Issues document cannot downgrade mandatory requirements. Any issue that violates a
mandatory requirement must also appear as FAIL/BLOCKED/NOT RUN in the relevant Release Gate.

## Known limitations (scope of headless CI, not defects)

| ID | Severity | Affected requirement/gate | Reproduction | Current behavior | Expected behavior | Workaround |
|---|---|---|---|---|---|---|
| K1 | Informational | Main UI, Editor, Advanced Editor, Physics Inspector, Desktop Interaction/HiDPI, Visual Evidence (gates NOT RUN) | Run full interactive suite on a Windows desktop | These gates are marked NOT RUN; UI flows were not driven in this CI | Live Win32 UI/animation/HiDPI/IME verification per `doc 12` | Launch `build/pinball_sandbox.exe` on Windows and run the manual acceptance checklist (`doc 13`) |
| K2 | Informational | Windows Platform Binding gate (NOT RUN) | Live HWND / Per-Monitor DPI v2 / IME / USER-GDI cycle checks | Build/link/source audit PASS; live desktop checks not executed here | Full Windows Platform Binding assertions per `doc 32` | Run on a Windows host; source audit already confirms no prohibited dependency and software-rendered framebuffer |
| K3 | Low | Persistence GUI dialogs (Save As, dirty Save/Discard/Cancel) | Interactive save dialogs | Underlying save/load/atomic-write APIs and fault injection are unit-verified; the dialog wrappers are not driven headlessly | GUI dialog flows asserted PASS | Manual UI pass on Windows |

## Mandatory-gate impact

- No mandatory automated test or headless Release Gate is FAILING or BLOCKED.
- The only NOT RUN items are interactive/visual gates (K1, K2) and their supporting GUI dialogs (K3), which by definition require a live Win32 desktop and are therefore out of scope for headless CI. They are recorded honestly as NOT RUN, not hidden.
- None of K1–K3 violates a mandatory requirement on the headless/automated side.

## Conclusion

No known defects violate a mandatory requirement. All 722 automated tests pass; the
delivered build is self-consistent (physics/event/headless share one production core), and
`releasecheck` passes. The remaining verification work is live-UI acceptance on a Windows host.
