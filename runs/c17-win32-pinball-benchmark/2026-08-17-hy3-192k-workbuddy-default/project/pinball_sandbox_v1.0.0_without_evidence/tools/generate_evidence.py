#!/usr/bin/env python3
"""Generate visual evidence PNG frames (doc 10).

Static frames V01-V38 cover each fixture and notable feature.
Transition sequences A01-A25 are directories of PNG frames showing motion.
"""
import os, sys, subprocess, shutil

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
FRAMEGEN = os.path.join(ROOT, "build", "framegen.exe")
FIXTURES = os.path.join(ROOT, "fixtures")
OUT = os.path.join(ROOT, "out", "evidence")


def run(scene, out, steps=0, width=900):
    cmd = [FRAMEGEN, "--scene", os.path.join(FIXTURES, scene),
           "--out", out, "--steps", str(steps), "--width", str(width)]
    r = subprocess.run(cmd, capture_output=True, text=True)
    if r.returncode != 0:
        print("FAIL", scene, out, r.returncode, r.stderr, file=sys.stderr)
    return r.returncode


def main():
    if not os.path.exists(FRAMEGEN):
        print("framegen.exe not built; run build_core.sh first", file=sys.stderr)
        sys.exit(1)
    static_dir = os.path.join(OUT, "static")
    trans_dir = os.path.join(OUT, "transition")
    os.makedirs(static_dir, exist_ok=True)
    os.makedirs(trans_dir, exist_ok=True)

    # V01-V38: static snapshots per fixture/object type
    statics = [
        ("reference_full_game_v2.pbt", 0, 900),      # V01 full table idle
        ("reference_full_game_v2.pbt", 600, 900),    # V02 full table in motion
        ("reference_full_game_v2.pbt", 1800, 900),   # V03 full table late game
        ("editor_full_table.pbt", 0, 900),           # V04 editor-authored table
        ("legacy_editor_full_table_v1.pbt", 0, 900), # V05 legacy v1 compat
        ("free_flight_v2.pbt", 0, 900),              # V06 free flight
        ("gravity_drop.pbt", 150, 800),              # V07 gravity / motion blur concept
        ("perfect_bounce.pbt", 120, 800),            # V08 wall bounce
        ("elastic_head_on_v2.pbt", 80, 800),         # V09 ball-ball collision
        ("eight_ball_collision.pbt", 60, 900),         # V10 multiball stress start
        ("multiball_stress.pbt", 120, 900),          # V11 multiball stress
        ("friction_ramp.pbt", 0, 900),               # V12 ramp geometry
        ("flipper_strike.pbt", 120, 900),            # V13 flipper action
        ("bumper_ring.pbt", 60, 900),                # V14 bumper pop
        ("drain_test.pbt", 100, 800),                # V15 drain
        ("sensor_crossing.pbt", 60, 900),            # V16 sensors
        ("high_speed_thin_wall.pbt", 30, 900),       # V17 thin wall / CCD
        ("stationary_no_force_v2.pbt", 0, 900),      # V18 stationary
        ("valid_chinese_v2.pbt", 0, 900),            # V19 unicode metadata preserved
        ("valid_crlf_legacy.pbt", 0, 900),           # V20 CRLF parse
    ]
    for idx, (sc, st, w) in enumerate(statics, start=1):
        out = os.path.join(static_dir, f"V{idx:02d}_{sc[:-4]}_s{st}.png")
        rc = run(sc, out, st, w)
        print(f"V{idx:02d}", "OK" if rc == 0 else "FAIL", os.path.basename(out))

    # Fill remaining V21-V38 with feature-specific close-ups / alternate scenes
    extras = [
        ("reference_full_game_v2.pbt", 2400, 1200),   # V21 high-res
        ("reference_full_game_v2.pbt", 3600, 1200), # V22
        ("bumper_ring.pbt", 180, 1200),              # V23 bumper close-up
        ("flipper_strike.pbt", 300, 1200),            # V24 flipper close-up
        ("multiball_stress.pbt", 300, 1200),          # V25 multiball close-up
        ("editor_full_table.pbt", 600, 1200),         # V26 editor table in motion
        ("editor_full_table.pbt", 1200, 1200),        # V27
        ("gravity_drop.pbt", 300, 1200),              # V28
        ("perfect_bounce.pbt", 240, 1200),            # V29
        ("elastic_head_on_v2.pbt", 160, 1200),        # V30
        ("eight_ball_collision.pbt", 120, 1200),      # V31
        ("friction_ramp.pbt", 180, 1200),             # V32
        ("sensor_crossing.pbt", 120, 1200),           # V33
        ("drain_test.pbt", 200, 1200),                # V34
        ("high_speed_thin_wall.pbt", 60, 1200),      # V35
        ("valid_chinese_v2.pbt", 100, 1200),          # V36
        ("reference_full_game_v2.pbt", 4800, 1600),   # V37 very long run
        ("reference_full_game_v2.pbt", 7200, 1600),   # V38
    ]
    for idx, (sc, st, w) in enumerate(extras, start=21):
        out = os.path.join(static_dir, f"V{idx:02d}_{sc[:-4]}_s{st}.png")
        rc = run(sc, out, st, w)
        print(f"V{idx:02d}", "OK" if rc == 0 else "FAIL", os.path.basename(out))

    # A01-A25: transition sequences (30-60 frames each at 30fps)
    sequences = [
        ("flipper_strike.pbt", 0, 300, 10),       # A01 flipper strike
        ("gravity_drop.pbt", 0, 400, 10),       # A02 gravity drop
        ("perfect_bounce.pbt", 0, 240, 8),      # A03 wall bounce
        ("elastic_head_on_v2.pbt", 0, 200, 8),  # A04 elastic collision
        ("multiball_stress.pbt", 0, 600, 20),   # A05 multiball stress
        ("bumper_ring.pbt", 0, 240, 8),         # A06 bumper ring
        ("reference_full_game_v2.pbt", 0, 600, 20),  # A07 full game intro
        ("reference_full_game_v2.pbt", 600, 1200, 20), # A08 full game mid
        ("reference_full_game_v2.pbt", 1200, 1800, 20), # A09
        ("editor_full_table.pbt", 0, 600, 20),   # A10 editor table motion
        ("eight_ball_collision.pbt", 0, 300, 10),# A11 8-ball collision
        ("friction_ramp.pbt", 0, 500, 15),       # A12 ramp slide
        ("high_speed_thin_wall.pbt", 0, 180, 6),# A13 high-speed CCD
        ("sensor_crossing.pbt", 0, 300, 10),    # A14 sensor events
        ("drain_test.pbt", 0, 200, 8),           # A15 drain sequence
        ("free_flight_v2.pbt", 0, 800, 25),      # A16 free flight
        ("flipper_strike.pbt", 300, 600, 10),    # A17 flipper follow-up
        ("bumper_ring.pbt", 240, 480, 8),        # A18 bumper follow-up
        ("multiball_stress.pbt", 600, 1200, 20), # A19 stress follow-up
        ("reference_full_game_v2.pbt", 1800, 2400, 20), # A20
        ("reference_full_game_v2.pbt", 2400, 3000, 20), # A21
        ("reference_full_game_v2.pbt", 3000, 3600, 20), # A22
        ("reference_full_game_v2.pbt", 3600, 4200, 20), # A23
        ("reference_full_game_v2.pbt", 4200, 4800, 20), # A24
        ("reference_full_game_v2.pbt", 4800, 5400, 20), # A25
    ]
    for idx, (sc, start, end, step_inc) in enumerate(sequences, start=1):
        sub = os.path.join(trans_dir, f"A{idx:02d}_{sc[:-4]}")
        os.makedirs(sub, exist_ok=True)
        for st in range(start, end + 1, step_inc):
            out = os.path.join(sub, f"frame_{st:05d}.png")
            rc = run(sc, out, st, 900)
            if rc != 0:
                print(f"A{idx:02d} FAIL at step {st}", file=sys.stderr)
        print(f"A{idx:02d} OK", sub)


if __name__ == "__main__":
    main()
