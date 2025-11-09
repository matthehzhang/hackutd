extends CharacterBody2D

class_name Player

"""
This implements a very rudimentary state machine. There are better implementations
in the AssetLib if you want to make something more complex.
"""

@export var WALK_SPEED: int = 150 # pixels per second
@export var ROLL_SPEED: int = 300 # pixels per second
@export var hitpoints: int = 20

var linear_vel = Vector2()
var roll_direction = Vector2.DOWN

signal health_changed(current_hp)

@export var facing = "down" # (String, "up", "down", "left", "right")

var despawn_fx = preload("res://scenes/misc/DespawnFX.tscn")

var anim = ""
var new_anim = ""

enum { STATE_BLOCKED, STATE_IDLE, STATE_WALKING, STATE_ATTACK, STATE_ROLL, STATE_DIE, STATE_HURT }

var state = STATE_IDLE

# Move the player to the corresponding spawnpoint, if any and connect to the dialog system
func _ready():
	var spawnpoints = get_tree().get_nodes_in_group("spawnpoints")
	for spawnpoint in spawnpoints:
		if spawnpoint.name == Globals.spawnpoint:
			global_position = spawnpoint.global_position
			break
	if not (
			Dialogs.dialog_started.connect(_on_dialog_started) == OK and
			Dialogs.dialog_ended.connect(_on_dialog_ended) == OK ):
		printerr("Error connecting to dialog system")
		
	setup_camera()	
	pass

func setup_camera():
	# Find the current room node
	var current_room = get_parent()  # Assumes player is child of room
	
	# Check if this room should have a fixed camera
	if current_room.is_in_group("fixed_camera"):
		# Center camera on room
		var room_center = get_room_center(current_room)
		# Detach camera from player movement
		$Camera2D.set_as_top_level(true)
		$Camera2D.global_position = room_center
		$Camera2D.position_smoothing_enabled = false
		# Detach camera from player movement
		
	else:
		# Camera follows player normally
		$Camera2D.set_as_top_level(false)
		$Camera2D.position_smoothing_enabled = true
		$Camera2D.position = Vector2.ZERO

func get_room_center(room):
	# Find a node marked as the room center, or use a default
	var center_markers = room.get_node("CameraCenter") if room.has_node("CameraCenter") else null
	if center_markers:
		return center_markers.global_position
	else:
		# Fallback: estimate from room size (adjust these values)
		return Vector2(0,0)

func _physics_process(_delta):
	
	## PROCESS STATES
	match state:
		STATE_BLOCKED:
			new_anim = "idle_" + facing
			pass
		STATE_IDLE:
			if (
					Input.is_action_pressed("move_down") or
					Input.is_action_pressed("move_left") or
					Input.is_action_pressed("move_right") or
					Input.is_action_pressed("move_up")
				):
					state = STATE_WALKING
			if Input.is_action_just_pressed("attack"):
				state = STATE_ATTACK
			if Input.is_action_just_pressed("roll"):
				state = STATE_ROLL
				roll_direction = Vector2(
						- int( Input.is_action_pressed("move_left") ) + int( Input.is_action_pressed("move_right") ),
						-int( Input.is_action_pressed("move_up") ) + int( Input.is_action_pressed("move_down") )
					).normalized()
				_update_facing()
			new_anim = "idle_" + facing
			pass
		STATE_WALKING:
			if Input.is_action_just_pressed("attack"):
				state = STATE_ATTACK
			if Input.is_action_just_pressed("roll"):
				state = STATE_ROLL
			
			set_velocity(linear_vel)
			move_and_slide()
			linear_vel = velocity
			
			var target_speed = Vector2()
			
			if Input.is_action_pressed("move_down"):
				target_speed += Vector2.DOWN
			if Input.is_action_pressed("move_left"):
				target_speed += Vector2.LEFT
			if Input.is_action_pressed("move_right"):
				target_speed += Vector2.RIGHT
			if Input.is_action_pressed("move_up"):
				target_speed += Vector2.UP
			
			target_speed *= WALK_SPEED
			#linear_vel = linear_vel.linear_interpolate(target_speed, 0.9)
			linear_vel = target_speed
			roll_direction = linear_vel.normalized()
			
			_update_facing()
			
			if linear_vel.length() > 5:
				new_anim = "walk_" + facing
			else:
				goto_idle()
			pass
		STATE_ATTACK:
			new_anim = "slash_" + facing
			pass
		STATE_ROLL:
			if roll_direction == Vector2.ZERO:
				state = STATE_IDLE
			else:
				set_velocity(linear_vel)
				move_and_slide()
				linear_vel = velocity
				var target_speed = Vector2()
				target_speed = roll_direction
				target_speed *= ROLL_SPEED
				#linear_vel = linear_vel.linear_interpolate(target_speed, 0.9)
				linear_vel = target_speed
				new_anim = "roll"
		STATE_DIE:
			new_anim = "die"
		STATE_HURT:
			new_anim = "hurt"
	
	## UPDATE ANIMATION
	if new_anim != anim:
		anim = new_anim
		$anims.play(anim)
	pass


func _on_dialog_started():
	state = STATE_BLOCKED

func _on_dialog_ended():
	state = STATE_IDLE


## HELPER FUNCS
func goto_idle():
	linear_vel = Vector2.ZERO
	new_anim = "idle_" + facing
	state = STATE_IDLE


func _update_facing():
	if Input.is_action_pressed("move_left"):
		facing = "left"
	if Input.is_action_pressed("move_right"):
		facing = "right"
	if Input.is_action_pressed("move_up"):
		facing = "up"
	if Input.is_action_pressed("move_down"):
		facing = "down"


func despawn():
	var despawn_particles = despawn_fx.instantiate()
	get_parent().add_child(despawn_particles)
	despawn_particles.global_position = global_position
	hide()
	await get_tree().create_timer(5.0).timeout
	get_tree().reload_current_scene()
	pass


func _on_hurtbox_area_entered(area):
	if state != STATE_DIE and area.is_in_group("enemy_weapons"):
		hitpoints -= 1
		emit_signal("health_changed", hitpoints)
		var pushback_direction = (global_position - area.global_position).normalized()
		set_velocity(pushback_direction * 5000)
		move_and_slide()
		state = STATE_HURT
		if hitpoints <= 0:
			state = STATE_DIE
	pass
