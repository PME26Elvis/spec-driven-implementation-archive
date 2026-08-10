PINBALL_TABLE 1
[table]
name = "Acceptance Gravity Drop"
world_size = (1600, 1000)
gravity = (0, 980)
max_active_balls = 16
starting_turns = 3
default_ball_radius = 12
default_ball_mass = 1
default_ball_restitution = 1
default_ball_friction = 0
default_ball_damping = 0
default_ball_max_speed = 3000
[object BALL_SPAWN spawn_main]
position = (500, 100)
initial_velocity = (0, 0)
enabled = true
[object DRAIN drain_main]
rect = (1400, 900, 100, 50)
enabled = true
