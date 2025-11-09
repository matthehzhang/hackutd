extends Node

var serial_manager
# Called when the node enters the scene tree for the first time.
func _ready() -> void:
	serial_manager = get_node("SerialManager")
	serial_manager.SendMessage("Hello ESP32!")
	serial_manager.SendMessage("HOUSE")
	pass # Replace with function body.


# Called every frame. 'delta' is the elapsed time since the previous frame.
func _process(delta: float) -> void:
	var msg = serial_manager.ReadMessage()
	if msg != "":
		print("Received from ESP:", msg)
	pass
