# Acceptance Evidence Index (partial)

## Automated tests
See `TEST_REGISTRY_REPORT.txt` — all executed cases PASS (0 failures).

## Features demonstrated in headless smoke
- Starter scene with static walls/floor, mixed dynamic bodies, distance-joint pendulum, bridge segments
- Fixed-step simulation, gravity, contacts, joints, CCD path for fast bodies
- Scene JSON save/load, undo stack, spatial query, sensors
- Solver trace export, replay capture

## UI features (require X11 display to visually verify)
- Custom software-rendered UI: panels, buttons, sliders, checkboxes, labels
- Frosted blur, animated capsule nav, inspector, modal
- Tool rail (SEL/CIR/RECT/FORCE), matrix grid, replay scrubber, motion trails

## Environment limitation
This CI/container environment has no active X11 display server; GUI screenshots and recordings cannot be captured here. Headless physics and unit/integration tests are the available evidence channel.

## Remaining for full package DoD
- Complete Golden Scenario 12/12 per doc 22
- Full media evidence pack
- Every advanced validation case at full depth from docs 15/17/24
