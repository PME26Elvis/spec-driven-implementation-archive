CC = gcc
CFLAGS = -std=c17 -Wall -Wextra -O2 -I src -D_DEFAULT_SOURCE -D_POSIX_C_SOURCE=200809L
LDFLAGS = -lm -lX11

PHYS_SRC = src/physics/shape.c src/physics/body.c src/physics/world.c \
           src/physics/collision.c src/physics/solver.c src/physics/bvh.c \
           src/physics/joint.c src/physics/contact_cache.c src/physics/ccd.c \
           src/physics/matrix.c src/physics/replay.c src/physics/query.c \
           src/physics/sensor.c
RENDER_SRC = src/render/framebuffer.c
SCENE_SRC = src/scene/scene.c src/scene/scene_io.c src/scene/undo.c
UI_SRC = src/ui/ui.c src/ui/ui_core.c
DIAG_SRC = src/diagnostics/trail.c src/diagnostics/solver_export.c
APP_SRC = src/app/main.c

.PHONY: all tests clean package

all: /tmp/physics_sandbox
/tmp/physics_sandbox: $(PHYS_SRC) $(RENDER_SRC) $(SCENE_SRC) $(UI_SRC) $(DIAG_SRC) $(APP_SRC)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

tests: test_math test_freefall test_circle_collision test_bvh test_joint test_stack \
	test_determinism test_manifold test_ccd test_motor test_scene_io test_golden \

define RUN_TEST
	$(CC) $(CFLAGS) -o /tmp/$(1) $(2) && /tmp/$(1)
endef

test_math:
	$(CC) $(CFLAGS) -o /tmp/test_math tests/unit/test_math.c -lm && /tmp/test_math
test_freefall:
	$(CC) $(CFLAGS) -o /tmp/test_freefall $(PHYS_SRC) tests/unit/test_freefall.c -lm && /tmp/test_freefall
test_circle_collision:
	$(CC) $(CFLAGS) -o /tmp/test_cc $(PHYS_SRC) tests/unit/test_circle_collision.c -lm && /tmp/test_cc
test_bvh:
	$(CC) $(CFLAGS) -o /tmp/test_bvh src/physics/bvh.c tests/unit/test_bvh.c -lm && /tmp/test_bvh
test_joint:
	$(CC) $(CFLAGS) -o /tmp/test_joint $(PHYS_SRC) tests/unit/test_joint.c -lm && /tmp/test_joint
test_stack:
	$(CC) $(CFLAGS) -o /tmp/test_stack $(PHYS_SRC) tests/unit/test_stack.c -lm && /tmp/test_stack
test_determinism:
	$(CC) $(CFLAGS) -o /tmp/test_det $(PHYS_SRC) tests/unit/test_determinism.c -lm && /tmp/test_det
test_manifold:
	$(CC) $(CFLAGS) -o /tmp/test_man $(PHYS_SRC) tests/unit/test_manifold.c -lm && /tmp/test_man
test_ccd:
	$(CC) $(CFLAGS) -o /tmp/test_ccd $(PHYS_SRC) tests/unit/test_ccd.c -lm && /tmp/test_ccd
test_motor:
	$(CC) $(CFLAGS) -o /tmp/test_motor $(PHYS_SRC) tests/unit/test_motor.c -lm && /tmp/test_motor
test_scene_io:
	$(CC) $(CFLAGS) -o /tmp/test_sio $(PHYS_SRC) src/scene/scene_io.c tests/unit/test_scene_io.c -lm && /tmp/test_sio
test_golden:
	$(CC) $(CFLAGS) -o /tmp/test_gold $(PHYS_SRC) tests/unit/test_golden_freefall.c -lm && /tmp/test_gold
test_matrix:
	$(CC) $(CFLAGS) -o /tmp/test_mat src/physics/matrix.c tests/unit/test_matrix.c -lm && /tmp/test_mat
test_replay:
	$(CC) $(CFLAGS) -o /tmp/test_rep $(PHYS_SRC) tests/unit/test_replay.c -lm && /tmp/test_rep
test_query:
	$(CC) $(CFLAGS) -o /tmp/test_q $(PHYS_SRC) tests/unit/test_query.c -lm && /tmp/test_q
test_undo:
	$(CC) $(CFLAGS) -o /tmp/test_u $(PHYS_SRC) src/scene/undo.c tests/unit/test_undo.c -lm && /tmp/test_u
test_golden_bounce:
	$(CC) $(CFLAGS) -o /tmp/test_gb $(PHYS_SRC) tests/unit/test_golden_bounce.c -lm && /tmp/test_gb
test_sensor:
	$(CC) $(CFLAGS) -o /tmp/test_sen $(PHYS_SRC) tests/unit/test_sensor.c -lm && /tmp/test_sen

clean:
	rm -f /tmp/physics_sandbox /tmp/test_*

package:
	cd /home/workdir/artifacts && rm -f physics_sandbox_v1.0_wip.zip && \
	zip -r physics_sandbox_v1.0_wip.zip physics_sandbox physics_sandbox_spec_v1.0 -x "*/build/*" -x "*.o" && ls -lh physics_sandbox_v1.0_wip.zip

test_solver_export:
	$(CC) $(CFLAGS) -o /tmp/test_se $(PHYS_SRC) src/diagnostics/solver_export.c tests/unit/test_solver_export.c -lm && /tmp/test_se

test_golden_bridge test_golden_restitution test_golden_static test_golden_filter:
	$(CC) $(CFLAGS) -o /tmp/test_gbr $(PHYS_SRC) tests/unit/test_golden_bridge test_golden_restitution test_golden_static test_golden_filter.c -lm && /tmp/test_gbr
test_golden_pendulum:
	$(CC) $(CFLAGS) -o /tmp/test_gp $(PHYS_SRC) tests/unit/test_golden_pendulum.c -lm && /tmp/test_gp
test_solver_export:
	$(CC) $(CFLAGS) -o /tmp/test_se $(PHYS_SRC) src/diagnostics/solver_export.c tests/unit/test_solver_export.c -lm && /tmp/test_se

test_golden_restitution:
	$(CC) $(CFLAGS) -o /tmp/test_gr $(PHYS_SRC) tests/unit/test_golden_restitution.c -lm && /tmp/test_gr
test_golden_static:
	$(CC) $(CFLAGS) -o /tmp/test_gst $(PHYS_SRC) tests/unit/test_golden_static.c -lm && /tmp/test_gst
test_golden_filter:
	$(CC) $(CFLAGS) -o /tmp/test_gf $(PHYS_SRC) tests/unit/test_golden_filter.c -lm && /tmp/test_gf

releasecheck:
	$(CC) $(CFLAGS) -o /tmp/releasecheck $(PHYS_SRC) $(SCENE_SRC) src/diagnostics/solver_export.c tests/release/releasecheck.c -lm && /tmp/releasecheck
