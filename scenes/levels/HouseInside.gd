extends Node2D

@export var auto_trigger_battle = true
@export var enemy_name = "Panther"
@export var enemy_health = 60
@export var enemy_attack = 10

var battle_triggered = false

func _ready():
	# If you have a dialog system, connect to it
	if Dialogs and not battle_triggered:
		# Wait a moment before starting cutscene
		# Debugging purposes
		print("started")
		await get_tree().create_timer(1).timeout
		if auto_trigger_battle:
			start_encounter()

func start_encounter():
	if battle_triggered:
		return
	battle_triggered = true
	
	# Freeze player movement during cutscene
	var player = get_tree().get_first_node_in_group("player")
	if player:
		player.set_physics_process(false)  # Disable player movement
	
	# Start dialogue cutscene
	# Adjust this based on how your Dialogs system works
	# This is a placeholder - you'll need to adapt it to your dialog system
	show_pre_battle_dialogue()

func show_pre_battle_dialogue():
	# If you have a Dialogs system, use it here
	# Example dialogue lines:
	var dialogue = "H...h..ehehehe...     Is that cat flesh I smell? Dinner time! Dinner time! DINNER! DINNERDINNERDINNERDINNERDINNERDINNERDINNERDINNERDINNER"

	# You'll need to adapt this to YOUR dialog system
	# For now, I'll show a simple approach
	await Dialogs.show_dialog(dialogue, enemy_name)
	await get_tree().create_timer(2.0).timeout
 
	# After dialogue, start battle
	start_battle()

func start_battle():
	# Store enemy data in Globals so battle scene can access it
	Globals.current_enemy_name = enemy_name
	Globals.current_enemy_health = enemy_health
	Globals.current_enemy_attack = enemy_attack
	Globals.current_enemy_attacks = ["Pounce"]
	Globals.battle_return_scene = "res://scenes/levels/Outside.tscn"
	Globals.spawnpoint = "house_outside"
	### REPLACE
	Globals.battle_background = "res://assets/backgrounds/abandoned_house_battle"
	
	# Transition to battle scene
	get_tree().change_scene_to_file("res://levels/battle.tscn")
