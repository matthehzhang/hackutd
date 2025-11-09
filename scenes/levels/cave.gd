extends Node

var serial_manager

# Called when the node enters the scene tree for the first time.
func _ready() -> void:
	await get_tree().create_timer(6.7).timeout
	get_tree().change_scene_to_file("res://scenes/levels/Fox_Fight.tscn")
	serial_manager.SendMessage("STONE_WALL")

# Called every frame. 'delta' is the elapsed time since the previous frame.
func _process(delta: float) -> void:
	pass
