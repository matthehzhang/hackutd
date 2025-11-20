extends Node

var serial_manager
# Called when the node enters the scene tree for the first time.
func _ready() -> void:
	serial_manager = get_node("SerialManager")
	serial_manager.SendMessage("Hello ESP32!")
	serial_manager.SendMessage("FEATHERS")
	Dialogs.show_dialog("Wow RAVEN, you fly sooo fast!", "Poyo")
	await get_tree().create_timer(6.7).timeout
	serial_manager.SendMessage("OFF")
	get_tree().change_scene_to_file("res://scenes/levels/Mountain.tscn")
	
	pass # Replace with function body.


# Called every frame. 'delta' is the elapsed time since the previous frame.
func _process(delta: float) -> void:
	var msg = serial_manager.ReadMessage()
	if msg != "":
		print("Received from ESP:", msg)
	pass
