PINBALL_TABLE 1
[table]
name = "Bumper Ring"
world_size = (1200, 900)
gravity = (0, 0)
max_active_balls = 4
starting_turns = 1
default_ball_radius = 12
default_ball_mass = 1
default_ball_restitution = 0.9
default_ball_friction = 0.02
default_ball_damping = 0
default_ball_max_speed = 4000
[object BALL_SPAWN spawn_main]
position = (600, 450)
initial_velocity = (480, 0)
enabled = true
[object BUMPER bumper_e]
center = (820, 450)
radius = 45
restitution = 0.9
friction = 0.02
impulse = 500
base_score = 100
cooldown = 0.10
enabled = true
[object BUMPER bumper_s]
center = (600, 670)
radius = 45
restitution = 0.9
friction = 0.02
impulse = 500
base_score = 100
cooldown = 0.10
enabled = true
[object BUMPER bumper_w]
center = (380, 450)
radius = 45
restitution = 0.9
friction = 0.02
impulse = 500
base_score = 100
cooldown = 0.10
enabled = true
[object BUMPER bumper_n]
center = (600, 230)
radius = 45
restitution = 0.9
friction = 0.02
impulse = 500
base_score = 100
cooldown = 0.10
enabled = true
[object WALL wall_top]
start = (250, 100)
end = (950, 100)
thickness = 20
restitution = 1
friction = 0
enabled = true
[object WALL wall_right]
start = (950, 100)
end = (950, 800)
thickness = 20
restitution = 1
friction = 0
enabled = true
[object WALL wall_bottom]
start = (950, 800)
end = (250, 800)
thickness = 20
restitution = 1
friction = 0
enabled = true
[object WALL wall_left]
start = (250, 800)
end = (250, 100)
thickness = 20
restitution = 1
friction = 0
enabled = true
[object DRAIN drain_far]
rect = (20, 850, 1160, 20)
enabled = true
