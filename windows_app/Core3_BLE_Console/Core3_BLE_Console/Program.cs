using System.Text;

using Raylib_cs;

namespace Core3_BLE_Console {
	internal class Program {
		static void Main(string[] args) {
			Console.WriteLine("Starting");

			Raylib.InitWindow(1680, 900, "Core3");
			//Raylib.SetWindowState(ConfigFlags.Msaa4xHint);
			//Raylib.SetWindowState(ConfigFlags.HighDpiWindow);
			Raylib.SetWindowState(ConfigFlags.VSyncHint);
			//Raylib.SetTargetFPS(240);

			Graphics.Init();
			Bluetooth.DoBluetooth();

			while (!Raylib.WindowShouldClose()) {
				Graphics.Draw();
			}

			Raylib.CloseWindow();
		}
	}
}
