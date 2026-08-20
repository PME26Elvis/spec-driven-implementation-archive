PINBALL_TABLE 1
[table]
name = "Acceptance High Speed Thin Wall"
world_size = (1600, 1000)
gravity = (0, 0)
max_active_balls = 16
starting_turns = 3
default_ball_radius = 12
default_ball_mass = 1
default_ball_restitution = 1
default_ball_friction = 0
default_ball_damping = 0
default_ball_max_speed = 10000
[object BALL_SPAWN spawn_main]
position = (800, 260)
initial_velocity = (0, 8000)
enabled = true
[object WALL wall_thin]
start = (300, 300)
end = (1300, 300)
thickness = 4
restitution = 1
friction = 0
enabled = true
[object DRAIN drain_main]
rect = (1400, 900, 100, 50)
enabled = true
