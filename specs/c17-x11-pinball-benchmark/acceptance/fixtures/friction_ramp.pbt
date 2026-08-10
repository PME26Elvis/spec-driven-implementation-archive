PINBALL_TABLE 1
[table]
name = "Friction Ramp"
world_size = (1600, 1000)
gravity = (0, 0)
max_active_balls = 4
starting_turns = 1
default_ball_radius = 12
default_ball_mass = 1
default_ball_restitution = 0.5
default_ball_friction = 0.8
default_ball_damping = 0
default_ball_max_speed = 3000
[object BALL_SPAWN spawn_main]
position = (500, 250)
initial_velocity = (300, 500)
enabled = true
[object RAMP ramp_test]
start = (250, 550)
end = (750, 450)
thickness = 20
restitution = 0.5
friction = 0.8
enabled = true
[object DRAIN drain_far]
rect = (100, 950, 1400, 40)
enabled = true
