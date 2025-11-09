extends Node2D

# References to UI elements - drag and drop in inspector or use get_node
@onready var enemy = $Enemy
@onready var player = $Player
#@onready var enemy_health_bar = $UI/EnemyHealthBar
#@onready var player_health_bar = $UI/PlayerHealthBar
@onready var battle_text = $UI/BattleText

# QTE variables
var qte_active = false
var qte_start_time = 0.0
var qte_min_time = 1.5  # Minimum time to wait
var qte_max_time = 1.75  # Maximum time allowed
var qte_success = false

# Battle state
var player_hp = 0
var player_max_hp = 0
var player_atk = 0
var enemy_hp = 0
var enemy_max_hp = 0
var enemy_atk = 0
var enemy_name = ""

var player_turn = true
var battle_active = true
var defending = false

func _ready():
	# For debugging
	print("battle started")
	# Load stats from Globals
	player_hp = 100
	player_max_hp = 100
	player_atk = 10
	
	enemy_name = "Panther"
	enemy_hp = 60
	enemy_max_hp = 60
	enemy_atk = 10
	
	# Setup UI
	#player_health_bar.max_value = player_max_hp
	#player_health_bar.value = player_hp
	#enemy_health_bar.max_value = enemy_max_hp
	#enemy_health_bar.value = enemy_hp
	
	# Start battle
	show_message(enemy_name + " is looking at you with bloodlust.")
	await get_tree().create_timer(1.5).timeout
	while player_hp > 0 and enemy_hp > 0 and battle_active: 
		await fight()

func show_message(text: String):
	battle_text.text = text
	print("hello")

func fight() -> void: 
	var success = false
	
	if enemy_name == "Panther": 
		show_message("Jump!")
		# line to send code to C# to then send serial code to active Jump LED wave
	if enemy_name == "Fox": 
		show_message("fox attack")
	if enemy_name == "The Mad Deer": 
		show_message("Think fast and follow along!")
		# line to send code to C# to then send serial code to activate arrows 
		# serial code should randomly choose one of the codes because ESP guy will make 
		# 5 diff variations and we will cycle them randomly
	success = await start_qte()
	qte_max_time -= .01
		
	if not success: 
		# animation for them attacking you 
		# Screen shake
		shake_sprite(player)
		player_hp -= enemy_atk
		player_hp = max(0, player_hp)
	
	#player_health_bar.value = player_hp
	
	await get_tree().create_timer(0.5).timeout
	
	# Check if player defeated
	if player_hp <= 0:
		lose_battle()
		return
	
	# Auto attack back
	enemy_hp -= player_atk
	enemy_hp = max(0, enemy_hp)
	shake_sprite(enemy)
	get_tree().create_timer(.5).timeout
	
	if enemy_hp <= 0: 
		win_battle()
		return
		
	await get_tree().create_timer(0.5).timeout

func start_qte():
	# Wait for minimum time first
	await get_tree().create_timer(qte_min_time).timeout
	
	# NOW the window opens
	qte_active = true
	qte_start_time = Time.get_ticks_msec() / 1000.0
	qte_success = false
	
	# Give them the window duration to press
	var window_duration = qte_max_time - qte_min_time
	await get_tree().create_timer(window_duration).timeout
	
	# Time's up if still active
	if qte_active:
		qte_active = false
		qte_success = false
	
	await get_tree().create_timer(0.5).timeout  # Small delay before continuing
	return qte_success

func win_battle():
	battle_active = false
	show_message("You saved " + enemy_name + "!")
	await get_tree().create_timer(2.0).timeout
	
	Globals.player_won_battle = true
	
	var dialogue = ""
	if enemy_name == "Panther": 
		dialogue = "Hero...thank you for saving my life! But you still have a journey ahead of you."
	if enemy_name == "Fox": 
		dialogue = "I don't know what came over me! I'm so upset I hurt Raven, one of my closest friends. Make sure you defeat the terrible deer."
	if enemy_name == "The Mad Deer": 
		dialogue = "Fine. You win. But don't expect this to end. You heroes trample on the lives of people like me, without ever questioning why we turned to this life in the first place."
		
	Dialogs.show_dialog(dialogue, enemy_name)
	await Dialogs.dialog_ended
	Dialogs.show_dialog("The Mad Deer will stop at nothing to annihilate this world. Head East: there is a cave where you shall continue your path.", enemy_name)
	await get_tree().create_timer(4.0).timeout
	
	# Raven thank you
	if enemy_name == "Fox":
		Dialogs.show_dialog("Fox is my best friend. Thank you for saving his life. Please let me repay the favor: I will take you to the top of the mountain where Mad Deer is!", "Raven")
		await get_tree().create_timer(4.0).timeout

	# Mad Deer monologue
	if enemy_name == "The Mad Deer": 
		Dialogs.show_dialog("The sheeple may call you a hero. But never forget: when we had to watch our brothers and sisters starve and our parents sell their lives just to send us to school, you were never once there.", "The Mad Deer")
		await get_tree().create_timer(6.0).timeout
		Dialogs.show_dialog("You disgust me. This won't be the last you hear from me. And children all over our world will continue to become me. Because after all, ", "The Mad Deer")
		await get_tree().create_timer(6.0).timeout
		Dialogs.show_dialog("i f  y o u  d o n ' t  c h a n ge  t h e  d u m p s t e r,  t h e  t r a s h  w i l l  n e v e r  s t o p  r o t t i n g", "?")
		
	return_to_scene(true)

func lose_battle():
	battle_active = false
	show_message("You died...")
	await get_tree().create_timer(2.0).timeout
	
	Globals.player_won_battle = false
	
	Dialogs.show_dialog("Please...try again. We're in so much pain...", enemy_name)
	
	return_to_scene(false)

func return_to_scene(won: bool):
	# Return to the scene you came from
	get_tree().change_scene_to_file("res://scenes/levels/Outside.tscn")

# Simple screen shake effect
func shake_sprite(sprite: Node):
	var original_pos = sprite.position
	for i in range(4):
		sprite.position = original_pos + Vector2(randf_range(-5, 5), randf_range(-5, 5))
		await get_tree().create_timer(0.05).timeout
	sprite.position = original_pos
	
# QTE
func _process(delta):
	if qte_active:
		if Input.is_action_just_pressed("ui_accept"):
			var time_elapsed = Time.get_ticks_msec() / 1000.0 - qte_start_time
			
			# Just check if within window (we already waited min_time before activating)
			if time_elapsed <= (qte_max_time - qte_min_time):
				qte_success = true
				qte_active = false
				show_message("Perfect!")  
			else:
				qte_success = false
				qte_active = false
				show_message("Too slow!")
