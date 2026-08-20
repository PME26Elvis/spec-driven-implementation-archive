PINBALL_TABLE 2

[table]
name = "Official Reference Table — 星際彈珠"
world_size = (1600, 1000)
gravity = (0, 980)
max_active_balls = 16
starting_turns = 3
default_ball_radius = 12
default_ball_mass = 1
default_ball_restitution = 0.78
default_ball_friction = 0.08
default_ball_damping = 0.03
default_ball_max_speed = 3000
scene_seed = 0
nudge_impulse = 85
nudge_tilt_cost = 1.0
tilt_threshold = 3.0
tilt_decay_per_second = 0.75
nudge_cooldown = 0.08

[layer layer_gameplay]
name = "Gameplay"
visible = true
locked = false
order = 0

[layer layer_guides]
name = "Guides"
visible = true
locked = false
order = 1

[layer layer_decor]
name = "Decorative"
visible = true
locked = false
order = 2

[object BALL_SPAWN spawn_main]
position = (1450, 850)
initial_velocity = (0, 0)
enabled = true
layer = layer_gameplay
locked = false

[object WALL wall_left]
start = (120, 120)
end = (120, 900)
thickness = 22
restitution = 0.82
friction = 0.06
enabled = true
layer = layer_gameplay
locked = false

[object WALL wall_right]
start = (1380, 120)
end = (1380, 900)
thickness = 22
restitution = 0.82
friction = 0.06
enabled = true
layer = layer_gameplay
locked = false

[object WALL wall_top]
start = (120, 120)
end = (1380, 120)
thickness = 22
restitution = 0.82
friction = 0.06
enabled = true
layer = layer_gameplay
locked = false

[object WALL wall_guide_locked]
start = (1280, 180)
end = (1360, 720)
thickness = 12
restitution = 0.72
friction = 0.05
enabled = true
layer = layer_guides
locked = true

[object RAMP ramp_left]
start = (260, 610)
end = (500, 500)
thickness = 16
restitution = 0.76
friction = 0.10
enabled = true
layer = layer_gameplay
locked = false

[object BUMPER bumper_1]
center = (540, 320)
radius = 44
restitution = 0.90
friction = 0.05
impulse = 500
base_score = 100
cooldown = 0.10
enabled = true
layer = layer_gameplay
locked = false

[object BUMPER bumper_2]
center = (760, 270)
radius = 44
restitution = 0.90
friction = 0.05
impulse = 500
base_score = 100
cooldown = 0.10
enabled = true
layer = layer_gameplay
locked = false

[object BUMPER bumper_3]
center = (980, 330)
radius = 44
restitution = 0.90
friction = 0.05
impulse = 500
base_score = 100
cooldown = 0.10
enabled = true
layer = layer_gameplay
locked = false

[object FLIPPER flipper_left]
pivot = (650, 790)
length = 160
thickness = 24
rest_angle_deg = 18
active_angle_deg = -28
engage_speed_deg_s = 900
return_speed_deg_s = 700
restitution = 0.85
friction = 0.10
input = LEFT_FLIPPER
enabled = true
layer = layer_gameplay
locked = false

[object FLIPPER flipper_right]
pivot = (950, 790)
length = 160
thickness = 24
rest_angle_deg = 162
active_angle_deg = 208
engage_speed_deg_s = 900
return_speed_deg_s = 700
restitution = 0.85
friction = 0.10
input = RIGHT_FLIPPER
enabled = true
layer = layer_gameplay
locked = false

[object SENSOR sensor_multiball]
rect = (700, 430, 200, 45)
enabled = true
debug_visible = false
layer = layer_gameplay
locked = false

[object DRAIN drain_main]
rect = (480, 940, 640, 55)
enabled = true
layer = layer_gameplay
locked = false

[object LAUNCHER launcher_main]
position = (1450, 850)
spawn = spawn_main
direction = (0, -1)
min_speed = 400
max_speed = 1800
full_charge_time = 1.2
charge_curve = LINEAR
enabled = true
layer = layer_gameplay
locked = false

[object ONE_WAY_GATE gate_upper]
start = (1090, 440)
end = (1230, 410)
thickness = 12
allowed_direction = (-0.2, -1)
restitution = 0.72
friction = 0.05
enabled = true
layer = layer_gameplay
locked = false

[object SLINGSHOT sling_left]
start = (390, 680)
end = (560, 745)
thickness = 18
restitution = 0.85
friction = 0.05
impulse = 350
base_score = 50
cooldown = 0.10
enabled = true
layer = layer_gameplay
locked = false

[object SLINGSHOT sling_right]
start = (1040, 745)
end = (1210, 680)
thickness = 18
restitution = 0.85
friction = 0.05
impulse = 350
base_score = 50
cooldown = 0.10
enabled = true
layer = layer_gameplay
locked = false

[object DROP_TARGET target_a]
start = (560, 470)
end = (620, 470)
thickness = 14
restitution = 0.72
friction = 0.08
min_hit_speed = 80
base_score = 250
cooldown = 0.10
initially_raised = true
reset_mode = ON_NEW_BALL
reset_delay = 0
enabled = true
layer = layer_gameplay
locked = false

[object DROP_TARGET target_b]
start = (650, 470)
end = (710, 470)
thickness = 14
restitution = 0.72
friction = 0.08
min_hit_speed = 80
base_score = 250
cooldown = 0.10
initially_raised = true
reset_mode = ON_NEW_BALL
reset_delay = 0
enabled = true
layer = layer_gameplay
locked = false

[object DROP_TARGET target_c]
start = (740, 470)
end = (800, 470)
thickness = 14
restitution = 0.72
friction = 0.08
min_hit_speed = 80
base_score = 250
cooldown = 0.10
initially_raised = true
reset_mode = ON_NEW_BALL
reset_delay = 0
enabled = true
layer = layer_gameplay
locked = false

[object STANDUP_TARGET standup_left]
start = (330, 390)
end = (390, 360)
thickness = 14
restitution = 0.75
friction = 0.07
min_hit_speed = 60
base_score = 150
cooldown = 0.10
enabled = true
layer = layer_gameplay
locked = false

[object STANDUP_TARGET standup_right]
start = (1110, 360)
end = (1170, 390)
thickness = 14
restitution = 0.75
friction = 0.07
min_hit_speed = 60
base_score = 150
cooldown = 0.10
enabled = true
layer = layer_gameplay
locked = false

[object ROLLOVER rollover_lane]
start = (1260, 300)
end = (1340, 300)
width = 24
base_score = 200
activation_mode = ON_ENTER
enabled = true
layer = layer_gameplay
locked = false

[object SPINNER spinner_center]
pivot = (850, 560)
half_length = 48
thickness = 10
rest_angle_deg = 90
angular_damping = 1.0
inertia = 1.0
restitution = 0.75
friction = 0.06
score_per_tick = 25
tick_angle_deg = 30
enabled = true
layer = layer_gameplay
locked = false

[object KICKOUT kickout_left]
center = (360, 260)
capture_radius = 26
eject_direction = (0.8, 0.6)
eject_speed = 900
hold_time = 0.75
base_score = 300
enabled = true
layer = layer_gameplay
locked = false

[group group_target_bank]
name = "三連靶"
pivot = (680, 470)
member_count = 3
member.0 = target_a
member.1 = target_b
member.2 = target_c

[event event_sensor_multiball]
source = sensor_multiball
trigger = SENSOR_ENTER
action_count = 3
action.0 = ADD_SCORE amount=500
action.1 = START_MULTIBALL spawn=spawn_main add_count=2
action.2 = LIGHT_INDICATOR target=bumper_2 duration=0.5

[event event_target_c_chain]
source = target_c
trigger = TARGET_DROPPED
action_count = 3
action.0 = ADD_SCORE amount=1000
action.1 = OPEN_GATE target=gate_upper duration=2.0
action.2 = START_MULTIBALL spawn=spawn_main add_count=1

[event event_rollover_bonus]
source = rollover_lane
trigger = ROLLOVER_ENTER
action_count = 1
action.0 = ADD_SCORE amount=200

[event event_kickout_capture]
source = kickout_left
trigger = KICKOUT_CAPTURE
action_count = 2
action.0 = ADD_SCORE amount=300
action.1 = LIGHT_INDICATOR target=bumper_1 duration=0.75
