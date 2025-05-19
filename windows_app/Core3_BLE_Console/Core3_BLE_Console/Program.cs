using System.Text;

using Raylib_cs;

namespace Core3_BLE_Console {
	class Program {
		public static int WinWidth;
		public static int WinHeight;

		static void Main(string[] args) {
			Console.WriteLine("Starting");

			WinWidth = 1680 + 300;
			WinHeight = 900 + 300;

			Bluetooth.DoBluetooth();

			Raylib.InitWindow(WinWidth, WinHeight, "Core3");
			Raylib.SetExitKey(KeyboardKey.Null);
			//Raylib.SetWindowState(ConfigFlags.Msaa4xHint);
			//Raylib.SetWindowState(ConfigFlags.HighDpiWindow);
			Raylib.SetWindowState(ConfigFlags.VSyncHint);
			//Raylib.SetTargetFPS(240);

			Graphics.Init();


			while (!Raylib.WindowShouldClose()) {
				Graphics.Update();
				Graphics.Draw();
			}

			Raylib.CloseWindow();
		}
	}
}
