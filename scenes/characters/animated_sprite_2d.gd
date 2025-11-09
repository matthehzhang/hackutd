extends AnimatedSprite2D

func _ready() -> void:
	# make sure an animation exists in SpriteFrames
	print(sprite_frames.get_animation_names())
	play("default")   # use a real animation name, not "default" unless you created it
	
# Called every frame. 'delta' is the elapsed time since the previous frame.
func _process(delta: float) -> void:
	pass
