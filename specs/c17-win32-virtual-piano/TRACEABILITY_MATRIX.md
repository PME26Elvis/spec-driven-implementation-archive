# Requirement Traceability Matrix

This matrix is a compact map, not a replacement for detailed specifications.

| Area | Primary requirements | Automated/manual tests | Release gate |
|---|---|---|---|
| C17/native boundary | ENG-001, ENG-010..011 | build/static review | G1, G8 |
| Custom renderer/UI | ENG-020..022, UI sections 2–20 | UI-001..016, VA-001..020 | G5, G6 |
| Piano range/pitch | PR-001..003 | PITCH-001..011 | G2, G5 |
| Octave/transpose | PR-010..022 | PITCH-001..009, MAN-003..004 | G2, G5 |
| Pointer/keyboard input | PR-030..045 | NOTE-001..009, MAN-008 | G2, G5 |
| Multi-source/voice pool | PR-050..061 | POLY-001..006 | G2, G4 |
| Sustain/release/volume | PR-070..091 | SUS-001..004, AUD-003..007, AA-005..008 | G2, G4, G5 |
| Chord recognition | PR-100..101 + chord spec | CR-001..034 + exhaustive matrix | G3 |
| Mapping/settings | PR-110..115 | MAP-001..016 | G9 |
| Recording | PR-120..126 | REC-001..015 | G10 |
| CLI/headless | PR-130..131 | CLI-001..014 | G11 |
| DPI/resize | PR-140..142 + DPI spec | DPI-001..012, VA-021..023 | G5, G6, G12 |
| Responsiveness/recovery | PR-150..152 + state model | NOTE/POLY/UI/manual cases | G2, G5 |
| Shutdown | PR-160 | AUD-013, REC-009, AA-010 | G4, G8, G10 |
| `locscan` | DEV_TOOLS entire spec | LOC-001..025 | G7 |
| Error/recovery | `ERROR_HANDLING_MATRIX.md` | mapped ERR + subsystem tests | relevant subsystem gate |
| Reference fixtures | `REFERENCE_FIXTURES.md`, `fixtures/` | CLI/MAP/LOC/AUD cases | G2,G3,G7,G9,G11 |
| Evidence/integrity | Delivery + testing sections | checklist J | G8 |

## Mandatory Evidence Mapping

| Evidence group | Minimum artifacts |
|---|---|
| Automated core | test runner summary with failing IDs if any |
| Chord | exhaustive generated count + CR regression summary |
| CLI | captured outputs/JSON for pitch, chord, diag, mapping validate, render |
| WAV | at least one offline deterministic fixture WAV + parsed header report |
| Live recording | recorded WAV when live audio/window environment permits |
| UI visual | VA screenshots/states when screenshot environment permits |
| DPI | 100/125/150 screenshots when possible + synthetic geometry test summary always |
| `locscan` | deterministic final JSON report + human summary |
| Manual | completed `ACCEPTANCE_CHECKLIST.md` copy/status report |

## Coverage Rule

A v1.0 submission shall not delete a requirement from its acceptance report because it was inconvenient to test.

Every release gate and every checklist section must have explicit status/evidence or an honest `UNVERIFIED` explanation.
