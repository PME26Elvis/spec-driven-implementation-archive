PINBALL_TABLE 1
[table]
name = "Acceptance Sensor Crossing"
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
[object SENSOR sensor_fast]
rect = (700, 270, 200, 5)
enabled = true
debug_visible = true
[object DRAIN drain_main]
rect = (1400, 900, 100, 50)
enabled = true
[event event_enter]
source = sensor_fast
trigger = SENSOR_ENTER
action_count = 1
action.0 = ADD_SCORE amount=10
[event event_leave]
source = sensor_fast
trigger = SENSOR_LEAVE
action_count = 1
action.0 = ADD_SCORE amount=20
