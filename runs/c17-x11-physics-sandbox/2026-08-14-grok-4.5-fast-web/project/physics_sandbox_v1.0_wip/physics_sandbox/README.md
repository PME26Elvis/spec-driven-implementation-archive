# Physics Sandbox v1.0 — C17 + X11 (Advanced WIP)

## Quick start
```
make tests
make all
/tmp/physics_sandbox   # headless if no DISPLAY
```

## Current verified capabilities
- Pure C17, X11 window + software renderer, headless fallback
- Math, shapes (circle/rect/convex), bodies, fixed-step world
- Dynamic AABB tree broad-phase
- Circle-circle, circle-poly/rect, basic SAT polygon-polygon
- Sequential impulse solver + friction + Baumgarte
- Distance / revolute / mouse joint definitions + velocity solve integrated
- Simple sleeping
- Unit tests all green (math, freefall, collision, bvh, joint)

## Remaining mandatory items (not claimed complete)
See evidence/STATUS.txt and the original 24 specification documents.
Full custom UI toolkit, CCD, complete joints with limits/motor/warm-start, 
scene I/O, Solver Inspector, Replay, Golden 12/12, full test registry + 
evidence package, etc. remain open.

No prohibited libraries or placeholder-only features used.
