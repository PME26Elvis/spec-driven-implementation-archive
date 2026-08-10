PINBALL_TABLE 1
[table]
name = "Flipper Strike"
world_size = (1200, 800)
gravity = (0, 0)
max_active_balls = 4
starting_turns = 1
default_ball_radius = 12
default_ball_mass = 1
default_ball_restitution = 0.8
default_ball_friction = 0.05
default_ball_damping = 0
default_ball_max_speed = 5000
[object BALL_SPAWN spawn_main]
position = (665, 420)
initial_velocity = (0, 0)
enabled = true
[object FLIPPER flipper_test]
pivot = (500, 500)
length = 180
thickness = 24
rest_angle_deg = 0
active_angle_deg = -50
engage_speed_deg_s = 1200
return_speed_deg_s = 900
restitution = 0.8
friction = 0.05
input = LEFT_FLIPPER
enabled = true
[object DRAIN drain_far]
rect = (50, 750, 1100, 30)
enabled = true
