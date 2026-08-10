PINBALL_TABLE 1
[table]
name = "Acceptance Full Object Table"
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
[object BALL_SPAWN spawn_main]
position = (1450, 850)
initial_velocity = (0, 0)
enabled = true
[object WALL wall_left]
start = (100, 100)
end = (100, 900)
thickness = 20
restitution = 0.8
friction = 0.05
enabled = true
[object RAMP ramp_001]
start = (300, 600)
end = (550, 500)
thickness = 16
restitution = 0.75
friction = 0.1
enabled = true
[object BUMPER bumper_001]
center = (800, 350)
radius = 45
restitution = 0.9
friction = 0.05
impulse = 500
base_score = 100
cooldown = 0.10
enabled = true
[object FLIPPER flipper_left]
pivot = (680, 780)
length = 150
thickness = 24
rest_angle_deg = 20
active_angle_deg = -25
engage_speed_deg_s = 900
return_speed_deg_s = 700
restitution = 0.85
friction = 0.1
input = LEFT_FLIPPER
enabled = true
[object SENSOR sensor_bonus]
rect = (700, 500, 200, 50)
enabled = true
debug_visible = false
[object DRAIN drain_main]
rect = (500, 940, 600, 50)
enabled = true
[object LAUNCHER launcher_main]
position = (1450, 850)
spawn = spawn_main
direction = (0, -1)
min_speed = 400
max_speed = 1800
full_charge_time = 1.2
charge_curve = LINEAR
enabled = true
[object ONE_WAY_GATE gate_001]
start = (1200, 400)
end = (1350, 400)
thickness = 12
allowed_direction = (0, -1)
restitution = 0.7
friction = 0.05
enabled = true
[object SLINGSHOT slingshot_001]
start = (450, 700)
end = (600, 760)
thickness = 18
restitution = 0.85
friction = 0.05
impulse = 350
base_score = 50
cooldown = 0.10
enabled = true
[event event_bonus]
source = sensor_bonus
trigger = SENSOR_ENTER
action_count = 3
action.0 = ADD_SCORE amount=500
action.1 = START_MULTIBALL spawn=spawn_main add_count=2
action.2 = LIGHT_INDICATOR target=bumper_001 duration=0.5
