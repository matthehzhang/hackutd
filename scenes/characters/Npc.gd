extends Area2D

"""
It just wraps around a sequence of dialogs. If it contains a child node named 'Quest'
which should be an instance of Quest.gd it'll become a quest giver and show whatever
text Quest.process() returns
"""

var active = false

@export var character_name: String = "Nameless NPC"
@export var dialogs = ["..."] # (Array, String, MULTILINE)
var current_dialog = 0

@onready var tooltip = $Tooltip

# Called when the node enters the scene tree for the first time.
func _ready():
	randomize()
	# warning-ignore:return_value_discarded
	body_entered.connect(_on_body_entered)
	# warning-ignore:return_value_discarded
	body_exited.connect(_on_body_exited)
	pass # Replace with function body.

func _input(event):
	# Bail if npc not active (player not inside the collider)
	if not active:
		return
	# Bail if Dialogs singleton is showing another dialog
	if Dialogs.active:
		return
	# Bail if the event is not a pressed "interact" action
	if not event.is_action_pressed("interact"):
		return
	
	if not dialogs.is_empty():
		for dialog in dialogs:
			Dialogs.show_dialog(dialog, character_name)
			await Dialogs.dialog_ended
		
func _on_body_entered(body):
	if body is Player:
		active = true
	tooltip.text = "Press space to talk"
		
func _on_body_exited(body):
	if body is Player:
		active = false
	tooltip.text = ""
