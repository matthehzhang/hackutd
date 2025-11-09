extends Node

@onready var tooltip = $UI/Tooltip
# Called when the node enters the scene tree for the first time.
func _ready() -> void:
	tooltip.text = "WASD to move"
	await get_tree().create_timer(10).timeout
	tooltip.text = ""
	pass # Replace with function body.


# Called every frame. 'delta' is the elapsed time since the previous frame.
func _process(delta: float) -> void:
	pass
