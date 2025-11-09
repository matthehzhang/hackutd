using Godot;
using System.IO.Ports;

public partial class Main : Node
{
	SerialPort port;

	public override void _Ready()
	{
		// Change COM port and baud rate as needed
		GD.Print("Available ports: ", string.Join(", ", SerialPort.GetPortNames()));

		port = new SerialPort("COM11", 115200);
		port.NewLine = "\n";
		port.Open();
		GD.Print("Connected to ESP32-S3!");
	}

	public override void _Process(double delta)
	{
		// Check for incoming messages
		if (port.IsOpen && port.BytesToRead > 0)
		{
			string line = port.ReadLine();
			GD.Print("From ESP: ", line);
		}

		// Example input to send commands
		if (Input.IsActionJustPressed("ui_accept")) // Enter key
		{
			port.Write("L");
			GD.Print("Sent LED ON");
		}
		if (Input.IsActionJustPressed("ui_cancel")) // Escape key
		{
			port.Write("l");
			GD.Print("Sent LED OFF");
		}
	}

	public override void _ExitTree()
	{
		if (port != null && port.IsOpen)
			port.Close();
	}
}
