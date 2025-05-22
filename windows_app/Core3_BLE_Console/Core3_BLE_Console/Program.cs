using System.Diagnostics;
using System.Numerics;
using System.Text;
using Raylib_cs;

namespace Core3_BLE_Console {
	class Program {
		static bool UseRT = true;

		static Vector2 RTPos;
		static RenderTexture2D RT;

		//static int WinWidth;
		//static int WinHeight;

		public static int ProgWidth;
		public static int ProgHeight;

		public static float GetScreenWidth() {
			return Raylib.GetScreenWidth();
		}

		public static float GetScreenHeight() {
			return Raylib.GetScreenHeight();
		}

		public static float ProgScreenWidth() {
			return ProgWidth;
		}

		public static float ProgScreenHeight() {
			return ProgHeight;
		}

		static Vector2 GetScreenScale() {
			float XScale = GetScreenWidth() / (float)ProgWidth;
			float YScale = GetScreenHeight() / (float)ProgHeight;
			float S = MathF.Min(XScale, YScale);

			return new Vector2(S, S);
		}

		public static Vector2 GetMousePosition() {
			Vector2 CurMouse = Raylib.GetMousePosition();
			Vector2 SScale = GetScreenScale();

			//Vector2 VirtMouse = new Vector2(0, 0);
			//VirtMouse.X = (CurMouse.X - (WinWidth - (ProgWidth * SScale.X)) * 0.5f) / SScale.X;
			//VirtMouse.Y = (CurMouse.Y - (WinHeight - (ProgHeight * SScale.Y)) * 0.5f) / SScale.Y;
			//VirtMouse = Vector2.Clamp(VirtMouse, new Vector2(0, 0), new Vector2(WinWidth, WinHeight));
			//return VirtMouse;

			return (CurMouse * SScale) - RTPos;
		}

		public static void Draw_RenderTexture(Action DrawAct) {
			if (UseRT) {
				Raylib.BeginTextureMode(RT);
				DrawAct();
				Raylib.EndTextureMode();
			} else {
				Raylib.BeginDrawing();
				DrawAct();
				Raylib.EndDrawing();
			}
		}

		public static void Draw() {
			if (!UseRT)
				return;

			Vector2 ScreenScale = GetScreenScale();

			Raylib.BeginDrawing();
			Raylib.ClearBackground(Color.Black);

			RTPos = new Vector2(
					(GetScreenWidth() - (ProgScreenWidth() * ScreenScale.X)) * 0.5f,
					(GetScreenHeight() - (ProgScreenHeight() * ScreenScale.Y)) * 0.5f
				);

			Rectangle SrcRec = new Rectangle(0.0f, 0.0f, RT.Texture.Width, -RT.Texture.Height);
			Rectangle DstRec = new Rectangle(RTPos.X, RTPos.Y, ProgScreenWidth() * ScreenScale.X, ProgScreenHeight() * ScreenScale.Y);


			Raylib.DrawTexturePro(RT.Texture, SrcRec, DstRec, Vector2.Zero, 0.0f, Color.White);
			Raylib.EndDrawing();
		}

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

			// Window size
			int WinWidth = 1680;
			int WinHeight = 900;

			// Render size
			ProgWidth = 1680;
			ProgHeight = 900;



			Bluetooth.DoBluetooth();

			Raylib.InitWindow(WinWidth, WinHeight, "Core3");
			Raylib.SetExitKey(KeyboardKey.Null);
			//Raylib.SetWindowState(ConfigFlags.Msaa4xHint);
			//Raylib.SetWindowState(ConfigFlags.HighDpiWindow);
			Raylib.SetWindowState(ConfigFlags.VSyncHint);
			Raylib.SetWindowState(ConfigFlags.ResizableWindow);
			Raylib.SetWindowMinSize(800, 600);
			Raylib.SetWindowMaxSize(5120, 1440);
			//Raylib.SetTargetFPS(240);

			RT = Raylib.LoadRenderTexture((int)ProgScreenWidth(), (int)ProgScreenHeight());
			Graphics.Init();


			while (!Raylib.WindowShouldClose()) {
				Graphics.Update();
				Graphics.Draw();
			}

			Raylib.CloseWindow();
		}
	}
}
