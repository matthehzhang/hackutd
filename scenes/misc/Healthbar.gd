extends HBoxContainer

"""
Connects to the player node and shows a health bar in the form of hearts
"""

#@onready var player: Node = $"../../Player"
#signal health_changed(new_hp: int)
var hitpoints: int = 100

var heart_scene = preload("res://scenes/misc/Heart.tscn")
# Called when the node enters the scene tree for the first time.
func _ready():
	#player.health_changed.connect(_on_health_changed)
	#_on_health_changed(player.hitpoints)
	pass # Replace with function body.


# You should probably rewrite this.
func _on_health_changed(new_hp):
	for child in get_children():
		child.queue_free()
	for i in new_hp:
		var heart = heart_scene.instantiate()
		add_child(heart)
	
