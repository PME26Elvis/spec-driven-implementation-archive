PINBALL_TABLE 1
[table]
name = "Base"
world_size = (1600, 1000)
gravity = (0, 0)
max_active_balls = 16
starting_turns = 3
default_ball_radius = 12
default_ball_mass = 1
default_ball_restitution = 1
default_ball_friction = 0
default_ball_damping = 0
default_ball_max_speed = 3000
[object BALL_SPAWN spawn_main]
position = (500, 500)
initial_velocity = (0, 0)
enabled = true
[object DRAIN drain_main]
rect = (1400, 900, 100, 50)
enabled = true
[object LAUNCHER launch]
position=(500,500)
spawn=drain_main
direction=(0,-1)
min_speed=400
max_speed=1800
full_charge_time=1.2
charge_curve=LINEAR
enabled=true
