# 10 — Visual Acceptance and Evidence Requirements

## 1. Principle

The task package specifies required visual evidence, not the screenshot/recording method. Evidence must come from the actual delivered build.

## 2. Evidence directory

Final deliverable contains `visual-evidence/` and `VISUAL_EVIDENCE.md` index.

Index maps every evidence ID to:

- file name;
- application version;
- table/fixture;
- relevant state/interaction;
- short requirement explanation.

## 3. Screenshot truthfulness

Images must:

- show actual running application;
- be readable;
- not be mockups;
- not be manually composited to hide missing UI;
- show relevant complete window region;
- correspond to delivered source/build.

## 4. Required static screenshots

- `V01` default Edit Mode at normal size;
- `V02` populated table showing all required object types or clearly represented authored instances;
- `V03` multiselection with handles and Inspector;
- `V04` Inspector numeric fields/sliders/toggles;
- `V05` validation panel containing Error and Warning examples;
- `V06` Play Mode single ball;
- `V07` Play Mode with at least 8 simultaneous balls;
- `V08` Physics Debug Overlay with contact normal and velocity vector;
- `V09` paused Inspector on selected runtime ball;
- `V10` launcher at partial charge;
- `V11` active combo + multiball HUD;
- `V12` fully-open modal with blurred/dimmed app background;
- `V13` collapsed sidebar;
- `V14` minimum supported window size;
- `V15` large/maximized layout;
- `V16` zoomed/panned editor;
- `V17` Save/Discard/Cancel dirty modal;
- `V18` replay playback state;
- `V19` game-over state;
- `V20` scene-load error presentation.

## 5. Required transition evidence

A screenshot cannot prove motion. Provide short recording, frame sequence, or equivalent observable evidence. Capture mechanism is not prescribed.

- `A01` hover elevation in/out;
- `A02` click ripple from click point;
- `A03` border-glow fade;
- `A04` Edit/Play capsule sliding;
- `A05` sidebar collapse/expand;
- `A06` modal scale+opacity+blur open;
- `A07` modal reversed before open completes;
- `A08` hover reversed before hover-in completes;
- `A09` frosted toolbar over changing/moving app canvas content;
- `A10` continuous resize with stable layout;
- `A11` flipper engage/release;
- `A12` launcher hold/charge/release;
- `A13` high-speed thin-wall collision without tunneling;
- `A14` multiball simulation;
- `A15` Pause → Single Step → Resume.

## 6. Manual visual consistency

Reviewer checks:

- no clipped required labels;
- no text outside modal/panel;
- no stale framebuffer after resize;
- no primary controls overlapping at minimum size;
- hover hit target matches visible control;
- collapsed panel has no invisible active targets;
- selection outline tracks object at all zooms;
- modal background actually blurred;
- ripple clipped to rounded control;
- shadows/glow do not create large opaque artifacts;
- debug overlay disappears cleanly when disabled;
- moving canvas under frosted toolbar does not reveal stale blur frames.

## 7. Visual acceptance failures

Fail even if logic works:

- native Win32 controls, Common Controls, native dialogs, or stock system widgets replace custom controls;
- modal appears instantly;
- hover changes only color;
- click feedback is a centered/static flash independent of click point;
- panel content teleports after width animation;
- blur replaced only by dark overlay;
- minimum-size layout makes required actions unreachable;
- heavy flicker during normal interaction;
- layout jitter from inconsistent rounding;
- keyboard focus indicator absent for primary controls.

## 8. Visual evidence is not functional proof

Passing screenshots/transitions does not prove physics, replay, persistence, or determinism. Automated Release Gates still apply.

## 9. v1.0 additional static screenshots

In addition to V01–V20, required:

- `V21` Layers/Groups/Lock UI with mixed selection;
- `V22` alignment/distribution plus exact Transform Inspector;
- `V23` Drop Target bank showing raised and dropped states;
- `V24` Spinner/Rollover/Kickout representative runtime states;
- `V25` Tilt active state;
- `V26` Event Trace populated by real event chain;
- `V27` Collision Trace for selected runtime ball;
- `V28` Scene Statistics in Edit Mode and Play fields visible;
- `V29` Chinese UTF-8 name after save/reload plus visible keyboard focus ring;
- `V30` Command Palette with filtered commands;
- `V31` 125% UI scale representative full window;
- `V32` 150% UI scale representative full window;
- `V33` 200% UI scale representative full window;
- `V34` autosave crash-recovery choice UI;
- `V35` external-modification conflict UI;
- `V36` migrated legacy-scene indication;
- `V37` official reference table in Edit Mode;
- `V38` official reference table in active multiball Play Mode.

## 10. v1.0 additional transition evidence

In addition to A01–A15, required:

- `A16` overlap selection cycling through stacked objects;
- `A17` group/layer lock preventing transform while unlocked members move;
- `A18` spinner rotating/ticking after ball contact;
- `A19` Kickout capture, hold, and eject;
- `A20` repeated nudge causing Tilt and flipper suppression;
- `A21` interrupted capsule/sidebar/modal sequence without snapping;
- `A22` UI scale change re-layout/re-rasterization without world-coordinate change;
- `A23` keyboard Tab/Shift+Tab focus traversal and modal focus trap;
- `A24` popup click-outside dismissal without background misactivation;
- `A25` resize/expose/close transitions with no stale framebuffer trail.

## 11. Required evidence range

For v1.0.0, the complete mandatory visual set is **V01–V38** and **A01–A25**. Capture method remains intentionally unspecified.


## Windows visual interpretation

The native Windows non-client title bar/frame is permitted and is not judged as part of the hand-built client UI. Every required surface inside the client area remains custom-rendered.

Existing HiDPI evidence V31–V33/A22 SHALL be captured on the Windows build with the effective DPI/UI-scale state recorded in the evidence index. At least one HiDPI evidence item must demonstrate a non-96-DPI window render rather than only the application's user-scale setting.

DWM-provided Acrylic/Mica/backdrop material does not count as evidence for the required software app-content blur. Evidence must show that the custom blur tracks content inside the application's own framebuffer.
