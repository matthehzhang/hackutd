using Godot;
using System.IO.Ports;

public partial class SerialManager : Node
{
	private SerialPort port;

	public override void _Ready()
	{
		GD.Print("Available ports: ", string.Join(", ", SerialPort.GetPortNames()));

		try
		{
			port = new SerialPort("COM11", 115200);
			port.Open();
			GD.Print("✅ Serial connected on COM11");
		}
		catch (System.Exception e)
		{
			GD.PrintErr("❌ Serial connection failed: " + e.Message);
		}
	}

	public void SendMessage(string message)
	{
		if (port != null && port.IsOpen)
		{
			port.WriteLine(message);
			GD.Print($"Sent: {message}");
		}
		else
		{
			GD.PrintErr("Serial port not open.");
		}
	}

	public string ReadMessage()
	{
		if (port != null && port.IsOpen && port.BytesToRead > 0)
			return port.ReadLine();
		return "";
	}

	public override void _ExitTree()
	{
		if (port != null && port.IsOpen)
			port.Close();
	}
}
