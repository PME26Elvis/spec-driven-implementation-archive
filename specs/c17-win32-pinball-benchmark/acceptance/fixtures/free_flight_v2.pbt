PINBALL_TABLE 2
[table]
name = "Free Flight"
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
scene_seed = 0
nudge_impulse = 85
nudge_tilt_cost = 1
tilt_threshold = 3
tilt_decay_per_second = 0.75
nudge_cooldown = 0.08
[layer gameplay]
name = "Gameplay"
visible = true
locked = false
order = 0
[object BALL_SPAWN spawn_main]
position = (500, 500)
initial_velocity = (120, -48)
enabled = true
layer = gameplay
locked = false
[object DRAIN drain_main]
rect = (1400, 900, 100, 50)
enabled = true
layer = gameplay
locked = false
