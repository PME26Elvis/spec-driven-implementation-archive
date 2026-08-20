PINBALL_TABLE 1
[table]
name = "Multiball Stress"
world_size = (1600, 1000)
gravity = (0, 650)
max_active_balls = 16
starting_turns = 1
default_ball_radius = 10
default_ball_mass = 1
default_ball_restitution = 0.82
default_ball_friction = 0.04
default_ball_damping = 0.02
default_ball_max_speed = 3500
[object BALL_SPAWN spawn_1]
position = (500, 250)
initial_velocity = (350, 50)
enabled = true
[object BALL_SPAWN spawn_2]
position = (600, 250)
initial_velocity = (-250, 150)
enabled = true
[object BALL_SPAWN spawn_3]
position = (700, 250)
initial_velocity = (200, 180)
enabled = true
[object BALL_SPAWN spawn_4]
position = (800, 250)
initial_velocity = (-180, 220)
enabled = true
[object BALL_SPAWN spawn_5]
position = (900, 250)
initial_velocity = (260, 80)
enabled = true
[object BALL_SPAWN spawn_6]
position = (1000, 250)
initial_velocity = (-320, 100)
enabled = true
[object BALL_SPAWN spawn_7]
position = (550, 350)
initial_velocity = (280, -100)
enabled = true
[object BALL_SPAWN spawn_8]
position = (650, 350)
initial_velocity = (-220, -80)
enabled = true
[object BALL_SPAWN spawn_9]
position = (750, 350)
initial_velocity = (190, 20)
enabled = true
[object BALL_SPAWN spawn_10]
position = (850, 350)
initial_velocity = (-290, 30)
enabled = true
[object BALL_SPAWN spawn_11]
position = (950, 350)
initial_velocity = (210, -40)
enabled = true
[object BALL_SPAWN spawn_12]
position = (1050, 350)
initial_velocity = (-240, -30)
enabled = true
[object BALL_SPAWN spawn_13]
position = (600, 450)
initial_velocity = (330, -120)
enabled = true
[object BALL_SPAWN spawn_14]
position = (750, 450)
initial_velocity = (-200, -130)
enabled = true
[object BALL_SPAWN spawn_15]
position = (900, 450)
initial_velocity = (230, -140)
enabled = true
[object BALL_SPAWN spawn_16]
position = (1050, 450)
initial_velocity = (-300, -110)
enabled = true
[object WALL wall_top]
start = (200, 100)
end = (1400, 100)
thickness = 20
restitution = 0.85
friction = 0.04
enabled = true
[object WALL wall_right]
start = (1400, 100)
end = (1400, 900)
thickness = 20
restitution = 0.85
friction = 0.04
enabled = true
[object WALL wall_bottom]
start = (1400, 900)
end = (200, 900)
thickness = 20
restitution = 0.85
friction = 0.04
enabled = true
[object WALL wall_left]
start = (200, 900)
end = (200, 100)
thickness = 20
restitution = 0.85
friction = 0.04
enabled = true
[object BUMPER bumper_a]
center = (600, 600)
radius = 45
restitution = 0.9
friction = 0.04
impulse = 500
base_score = 100
cooldown = 0.10
enabled = true
[object BUMPER bumper_b]
center = (800, 650)
radius = 45
restitution = 0.9
friction = 0.04
impulse = 500
base_score = 100
cooldown = 0.10
enabled = true
[object BUMPER bumper_c]
center = (1000, 600)
radius = 45
restitution = 0.9
friction = 0.04
impulse = 500
base_score = 100
cooldown = 0.10
enabled = true
[object SENSOR sensor_mid]
rect = (500, 520, 600, 40)
enabled = true
debug_visible = false
[object DRAIN drain_far]
rect = (210, 870, 1180, 20)
enabled = true
[event event_sensor]
source = sensor_mid
trigger = SENSOR_ENTER
action_count = 1
action.0 = LIGHT_INDICATOR target=bumper_b duration=0.15
