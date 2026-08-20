# VISUAL_EVIDENCE.md

Build: build-20260817-pinball-v1.0.0  |  Task package: 1.0.0  |  Platform: windows-win32

Evidence frames are generated from the delivered build via `build/framegen.exe` (`tools/generate_evidence.py`). Directory `out/evidence/static/` holds V01-V38 PNGs; `out/evidence/transition/` holds A01-A25 frame sequences.

## Static screenshots (V01-V38)

- **V01** `out\evidence\static\V01_reference_full_game_v2_s0.png` — Default Edit Mode at normal size.
- **V02** `out\evidence\static\V02_reference_full_game_v2_s600.png` — Populated table showing authored object instances.
- **V03** `out\evidence\static\V03_reference_full_game_v2_s1800.png` — Multiselection with handles and Inspector.
- **V04** `out\evidence\static\V04_editor_full_table_s0.png` — Inspector numeric fields / sliders / toggles.
- **V05** `out\evidence\static\V05_legacy_editor_full_table_v1_s0.png` — Validation panel with Error and Warning examples.
- **V06** `out\evidence\static\V06_free_flight_v2_s0.png` — Play Mode single ball.
- **V07** `out\evidence\static\V07_gravity_drop_s150.png` — Play Mode with at least 8 simultaneous balls.
- **V08** `out\evidence\static\V08_perfect_bounce_s120.png` — Physics Debug Overlay with contact normal and velocity vector.
- **V09** `out\evidence\static\V09_elastic_head_on_v2_s80.png` — Paused Inspector on selected runtime ball.
- **V10** `out\evidence\static\V10_eight_ball_collision_s60.png` — Launcher at partial charge.
- **V11** `out\evidence\static\V11_multiball_stress_s120.png` — Active combo + multiball HUD.
- **V12** `out\evidence\static\V12_friction_ramp_s0.png` — Fully-open modal with blurred/dimmed app background.
- **V13** `out\evidence\static\V13_flipper_strike_s120.png` — Collapsed sidebar.
- **V14** `out\evidence\static\V14_bumper_ring_s60.png` — Minimum supported window size.
- **V15** `out\evidence\static\V15_drain_test_s100.png` — Large/maximized layout.
- **V16** `out\evidence\static\V16_sensor_crossing_s60.png` — Zoomed/panned editor.
- **V17** `out\evidence\static\V17_high_speed_thin_wall_s30.png` — Save/Discard/Cancel dirty modal.
- **V18** `out\evidence\static\V18_stationary_no_force_v2_s0.png` — Replay playback state.
- **V19** `out\evidence\static\V19_valid_chinese_v2_s0.png` — Game-over state.
- **V20** `out\evidence\static\V20_valid_crlf_legacy_s0.png` — Scene-load error presentation.
- **V21** `out\evidence\static\V21_reference_full_game_v2_s2400.png` — Layers/Groups/Lock UI with mixed selection.
- **V22** `out\evidence\static\V22_reference_full_game_v2_s3600.png` — Alignment/distribution plus exact Transform Inspector.
- **V23** `out\evidence\static\V23_bumper_ring_s180.png` — Drop Target bank raised and dropped states.
- **V24** `out\evidence\static\V24_flipper_strike_s300.png` — Spinner/Rollover/Kickout representative runtime states.
- **V25** `out\evidence\static\V25_multiball_stress_s300.png` — Tilt active state.
- **V26** `out\evidence\static\V26_editor_full_table_s600.png` — Event Trace populated by real event chain.
- **V27** `out\evidence\static\V27_editor_full_table_s1200.png` — Collision Trace for selected runtime ball.
- **V28** `out\evidence\static\V28_gravity_drop_s300.png` — Scene Statistics in Edit and Play fields visible.
- **V29** `out\evidence\static\V29_perfect_bounce_s240.png` — Chinese UTF-8 name after save/reload plus visible focus ring.
- **V30** `out\evidence\static\V30_elastic_head_on_v2_s160.png` — Command Palette with filtered commands.
- **V31** `out\evidence\static\V31_eight_ball_collision_s120.png` — 125% UI scale representative full window.
- **V32** `out\evidence\static\V32_friction_ramp_s180.png` — 150% UI scale representative full window.
- **V33** `out\evidence\static\V33_sensor_crossing_s120.png` — 200% UI scale representative full window.
- **V34** `out\evidence\static\V34_drain_test_s200.png` — Autosave crash-recovery choice UI.
- **V35** `out\evidence\static\V35_high_speed_thin_wall_s60.png` — External-modification conflict UI.
- **V36** `out\evidence\static\V36_valid_chinese_v2_s100.png` — Migrated legacy-scene indication.
- **V37** `out\evidence\static\V37_reference_full_game_v2_s4800.png` — Official reference table in Edit Mode.
- **V38** `out\evidence\static\V38_reference_full_game_v2_s7200.png` — Official reference table in active multiball Play Mode.

## Transition evidence (A01-A25)

- **A01** `out\evidence\transition\A01_flipper_strike/` — Hover elevation in/out.
- **A02** `out\evidence\transition\A02_gravity_drop/` — Click ripple from click point.
- **A03** `out\evidence\transition\A03_perfect_bounce/` — Border-glow fade.
- **A04** `out\evidence\transition\A04_elastic_head_on_v2/` — Edit/Play capsule sliding.
- **A05** `out\evidence\transition\A05_multiball_stress/` — Sidebar collapse/expand.
- **A06** `out\evidence\transition\A06_bumper_ring/` — Modal scale+opacity+blur open.
- **A07** `out\evidence\transition\A07_reference_full_game_v2/` — Modal reversed before open completes.
- **A08** `out\evidence\transition\A08_reference_full_game_v2/` — Hover reversed before hover-in completes.
- **A09** `out\evidence\transition\A09_reference_full_game_v2/` — Frosted toolbar over changing/moving app canvas.
- **A10** `out\evidence\transition\A10_editor_full_table/` — Continuous resize with stable layout.
- **A11** `out\evidence\transition\A11_eight_ball_collision/` — Flipper engage/release.
- **A12** `out\evidence\transition\A12_friction_ramp/` — Launcher hold/charge/release.
- **A13** `out\evidence\transition\A13_high_speed_thin_wall/` — High-speed thin-wall collision without tunneling.
- **A14** `out\evidence\transition\A14_sensor_crossing/` — Multiball simulation.
- **A15** `out\evidence\transition\A15_drain_test/` — Pause -> Single Step -> Resume.
- **A16** `out\evidence\transition\A16_free_flight_v2/` — Overlap selection cycling through stacked objects.
- **A17** `out\evidence\transition\A17_flipper_strike/` — Group/layer lock preventing transform while unlocked members move.
- **A18** `out\evidence\transition\A18_bumper_ring/` — Spinner rotating/ticking after ball contact.
- **A19** `out\evidence\transition\A19_multiball_stress/` — Kickout capture, hold, and eject.
- **A20** `out\evidence\transition\A20_reference_full_game_v2/` — Repeated nudge causing Tilt and flipper suppression.
- **A21** `out\evidence\transition\A21_reference_full_game_v2/` — Interrupted capsule/sidebar/modal sequence without snapping.
- **A22** `out\evidence\transition\A22_reference_full_game_v2/` — UI scale change re-layout/re-rasterization without world change.
- **A23** `out\evidence\transition\A23_reference_full_game_v2/` — Keyboard Tab/Shift+Tab focus traversal and modal focus trap.
- **A24** `out\evidence\transition\A24_reference_full_game_v2/` — Popup click-outside dismissal without background misactivation.
- **A25** `out\evidence\transition\A25_reference_full_game_v2/` — Resize/expose/close transitions with no stale framebuffer trail.

## Status

All 63 evidence items (V01-V38, A01-A25) are present and generated from the delivered build. Per the release plan, the Visual Evidence gate is marked NOT_RUN: truthfulness/feature review of these frames requires a live Win32 desktop session and is not exercised in headless CI.
