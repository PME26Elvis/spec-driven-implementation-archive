PINBALL_TABLE 1
[table]
name = "Eight Ball Collision"
world_size = (1000, 800)
gravity = (0, 0)
max_active_balls = 16
starting_turns = 1
default_ball_radius = 16
default_ball_mass = 1
default_ball_restitution = 0.9
default_ball_friction = 0.02
default_ball_damping = 0.01
default_ball_max_speed = 3000
[object BALL_SPAWN spawn_1]
position = (300, 300)
initial_velocity = (300, 220)
enabled = true
[object BALL_SPAWN spawn_2]
position = (700, 300)
initial_velocity = (-300, 220)
enabled = true
[object BALL_SPAWN spawn_3]
position = (300, 500)
initial_velocity = (300, -220)
enabled = true
[object BALL_SPAWN spawn_4]
position = (700, 500)
initial_velocity = (-300, -220)
enabled = true
[object BALL_SPAWN spawn_5]
position = (500, 220)
initial_velocity = (0, 360)
enabled = true
[object BALL_SPAWN spawn_6]
position = (500, 580)
initial_velocity = (0, -360)
enabled = true
[object BALL_SPAWN spawn_7]
position = (240, 400)
initial_velocity = (420, 0)
enabled = true
[object BALL_SPAWN spawn_8]
position = (760, 400)
initial_velocity = (-420, 0)
enabled = true
[object WALL wall_top]
start = (150, 120)
end = (850, 120)
thickness = 20
restitution = 0.9
friction = 0.02
enabled = true
[object WALL wall_right]
start = (850, 120)
end = (850, 680)
thickness = 20
restitution = 0.9
friction = 0.02
enabled = true
[object WALL wall_bottom]
start = (850, 680)
end = (150, 680)
thickness = 20
restitution = 0.9
friction = 0.02
enabled = true
[object WALL wall_left]
start = (150, 680)
end = (150, 120)
thickness = 20
restitution = 0.9
friction = 0.02
enabled = true
[object DRAIN drain_far]
rect = (20, 740, 960, 20)
enabled = true
