using System.Diagnostics;
using System.Text;

using Raylib_cs;

namespace Core3_BLE_Console {
	class Program {
		public static int WinWidth;
		public static int WinHeight;

		static void Main(string[] args) {
			Console.WriteLine("Starting");

			/*Stopwatch SWatch = Stopwatch.StartNew();
			float Phase = 0.7f;

			while (true) {
				long MS = SWatch.ElapsedMilliseconds;

				float Val = MathF.Sin(MS / 1000.0f * Phase);

				Val = MathF.Round(Val, 2);
				Console.WriteLine("Val = {0}", Val);

				Thread.Sleep(1);
			}*/

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
