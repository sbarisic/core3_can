using System.Diagnostics;
using System.Numerics;
using System.Text;

using Raylib_cs;

namespace Core3_BLE_Console {
	class Program {
		static bool UseRT = true;

		static Vector2 RTPos;
		static Vector2 RTScale;
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

			return (CurMouse - RTPos) / RTScale;
		}

		static void CalcScalePos() {
			RTScale = GetScreenScale();

			RTPos = new Vector2(
					(GetScreenWidth() - (ProgScreenWidth() * RTScale.X)) * 0.5f,
					(GetScreenHeight() - (ProgScreenHeight() * RTScale.Y)) * 0.5f
				);
		}

		static void OnResize() {
			CalcScalePos();
			Vector2 NewRTSize = new Vector2(ProgScreenWidth() * RTScale.X, ProgScreenHeight() * RTScale.Y);

			if ((int)NewRTSize.X != ProgWidth && (int)NewRTSize.Y != ProgHeight) {
				ProgWidth = (int)MathF.Round(NewRTSize.X);
				ProgHeight = (int)MathF.Round(NewRTSize.Y);

				Console.WriteLine("NewSize X {0}, Y {1}", ProgWidth, ProgHeight);

				Raylib.UnloadRenderTexture(RT);
				RT = Raylib.LoadRenderTexture(ProgWidth, ProgHeight);
				Raylib.SetTextureFilter(RT.Texture, TextureFilter.Point);
			}

			CalcScalePos();
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


			Raylib.BeginDrawing();
			Raylib.ClearBackground(Color.Black);

			CalcScalePos();
			Rectangle SrcRec = new Rectangle(0.0f, 0.0f, RT.Texture.Width, -RT.Texture.Height);
			Rectangle DstRec = new Rectangle(RTPos.X, RTPos.Y, ProgScreenWidth() * RTScale.X, ProgScreenHeight() * RTScale.Y);


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
			int WinWidth = 1900;
			int WinHeight = 980;

			// Render size
			ProgWidth = WinWidth;
			ProgHeight = WinHeight;

			float Ratio = (float)WinHeight / WinWidth;

			Bluetooth.DoBluetooth();

			Raylib.InitWindow(WinWidth, WinHeight, "Core3");
			Raylib.SetExitKey(KeyboardKey.Null);
			Raylib.SetWindowState(ConfigFlags.Msaa4xHint);
			Raylib.SetWindowState(ConfigFlags.HighDpiWindow);
			Raylib.SetWindowState(ConfigFlags.VSyncHint);
			Raylib.SetWindowState(ConfigFlags.ResizableWindow);
			Raylib.SetWindowMinSize(800, (int)(800 * Ratio));
			Raylib.SetWindowMaxSize(5120, (int)(5120 * Ratio));
			//Raylib.SetTargetFPS(240);

			RT = Raylib.LoadRenderTexture((int)ProgScreenWidth(), (int)ProgScreenHeight());
			Raylib.SetTextureFilter(RT.Texture, TextureFilter.Point);
			//Raylib.SetTextureFilter(RT.Texture, TextureFilter.Point);
			Graphics.Init();


			while (!Raylib.WindowShouldClose()) {
				if (Raylib.IsWindowResized()) {
					OnResize();
				}

				Graphics.Update();
				Graphics.Draw();
			}

			Raylib.CloseWindow();
		}
	}
}
