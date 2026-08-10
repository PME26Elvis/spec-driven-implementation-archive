PINBALL_TABLE 1
[table]
name = "Acceptance High Speed Drain"
world_size = (1600, 1000)
gravity = (0, 0)
max_active_balls = 16
starting_turns = 3
default_ball_radius = 4
default_ball_mass = 1
default_ball_restitution = 1
default_ball_friction = 0
default_ball_damping = 0
default_ball_max_speed = 10000
[object BALL_SPAWN spawn_main]
position = (800, 250)
initial_velocity = (0, 10000)
enabled = true
[object DRAIN drain_main]
rect = (700, 270, 200, 5)
enabled = true
