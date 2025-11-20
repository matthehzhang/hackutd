extends Node

var serial_manager

# Called when the node enters the scene tree for the first time.
func _ready() -> void:
	serial_manager = get_node("SerialManager")
	serial_manager.SendMessage("STONE_WALL")
	Dialogs.show_dialog("I hope this minecart doesn't kill me!", "Poyo")
	await get_tree().create_timer(6.7).timeout
	get_tree().change_scene_to_file("res://scenes/levels/Fox_Fight.tscn")
		

# Called every frame. 'delta' is the elapsed time since the previous frame.
func _process(delta: float) -> void:
	pass
