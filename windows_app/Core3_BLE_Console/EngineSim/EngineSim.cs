using Raylib_cs;

using System.Diagnostics;
using System.Numerics;

using Windows.ApplicationModel.Contacts;

namespace EngineSim {
	internal class EngineSim {
		static Gfx Gfx;
		static GUI Gui;

		static Engine Engine;

		static void Main(string[] args) {
			int WinWidth = 1920;
			int WinHeight = 1080;
			float Ratio = (float)WinHeight / WinWidth;

			Raylib.InitWindow(WinWidth, WinHeight, "Core3");
			Raylib.SetExitKey(KeyboardKey.Null);
			Raylib.SetWindowState(ConfigFlags.Msaa4xHint);
			Raylib.SetWindowState(ConfigFlags.HighDpiWindow);
			Raylib.SetWindowState(ConfigFlags.VSyncHint);
			Raylib.SetWindowState(ConfigFlags.ResizableWindow);
			Raylib.SetWindowMinSize(800, (int)(800 * Ratio));
			Raylib.SetWindowMaxSize(5120, (int)(5120 * Ratio));
			Raylib.SetTargetFPS(240);

			Engine = new Engine();

			Gfx = new Gfx();
			Gfx.Width = Raylib.GetScreenWidth();
			Gfx.Height = Raylib.GetScreenHeight();

			Gui = new GUI();

			GUIPanel Tst = new GUIPanel();
			Tst.Position = new Vector2(80, 50);
			Tst.Size = new Vector2(600, 950);
			Tst.IsResizable = true;
			Gui.Add(Tst);

			int ChrIdx = 0;
			float TimeRange = 10;
			GUIChart Chr_RPM = AddChart(Tst, "RPM", TimeRange, ChrIdx++, Color.Red, 0, 7000);
			Chr_RPM.GetValue = () => Engine.RPM;

			GUIChart Chr_MAP = AddChart(Tst, "MAP", TimeRange, ChrIdx++, Color.SkyBlue, 0, 200);
			Chr_MAP.GetValue = () => Engine.MAP;

			GUIChart Chr_Ped = AddChart(Tst, "Pedal / DBW", TimeRange, ChrIdx++, Color.Green, 0, 100);
			Chr_Ped.GetValue = () => Engine.PedalPos;
			Chr_Ped.GetValue2 = () => Engine.DBWPos;
			Chr_Ped.Line2Color = Color.DarkGreen;
			Chr_Ped.UseSecondSamples = true;

			GUIChart Chr_TLam = AddChart(Tst, "TgtLam", TimeRange, ChrIdx++, Color.Orange, 0.5f, 1.5f);
			Chr_TLam.GetValue = () => Engine.TargetLambda;

			GUIChart Chr_AirF = AddChart(Tst, "AirFlow", TimeRange, ChrIdx++, Color.Yellow, 0, 200);
			Chr_AirF.GetValue = () => Engine.AirFlow;

			Thread UpdateEngineThread = new Thread(UpdateEngine);
			UpdateEngineThread.IsBackground = true;
			UpdateEngineThread.Start();

			Stopwatch SWatch = Stopwatch.StartNew();
			long LastTime = 0;
			float Dt = 0;
			float AccelPos = 0;

			float TargetRPM = 1500;

			while (!Raylib.WindowShouldClose()) {
				if (Raylib.IsWindowResized()) {
					OnResize();
				}

				Dt = (SWatch.ElapsedMilliseconds - LastTime) / 1000.0f;
				LastTime = SWatch.ElapsedMilliseconds;

				if (Raylib.IsKeyDown(KeyboardKey.PageUp)) {
					AccelPos += 30 * Dt;
				} else if (Raylib.IsKeyDown(KeyboardKey.PageDown)) {
					AccelPos -= 30 * Dt;
				} else if (Raylib.IsKeyDown(KeyboardKey.KpAdd)) {
					TargetRPM += 2000 * Dt;
				} else if (Raylib.IsKeyDown(KeyboardKey.KpSubtract)) {
					TargetRPM -= 2000 * Dt;
				}

				if (AccelPos > 100)
					AccelPos = 100;

				if (AccelPos < 0)
					AccelPos = 0;

				if (TargetRPM > 6500)
					TargetRPM = 6500;

				if (TargetRPM < 900)
					TargetRPM = 900;

				Engine.SetTargetRPM(TargetRPM);
				Engine.Pedal(AccelPos);

				Gui.Update();
				Gfx.Update();

				Gfx.BeginDraw();
				Gui.Draw(Gfx);
				Gfx.EndDraw();
			}

			Raylib.CloseWindow();
		}

		static GUIChart AddChart(GUIPanel Parent, string Name, float TimeRange, int Idx, Color LineClr, float Min, float Max) {
			GUIChart Chr1 = new GUIChart(Name, 200, TimeRange, Min, Max);
			Chr1.Position = new Vector2(10, (Chr1.Size.Y) * Idx);
			Chr1.Anchor = Parent;
			Chr1.LineColor = LineClr;
			Parent.Add(Chr1);
			return Chr1;
		}

		static void UpdateEngine() {
			Stopwatch SWatch = Stopwatch.StartNew();

			while (true) {
				Engine.Update(SWatch.ElapsedMilliseconds / 1000.0f);
				Thread.Sleep(10);
			}
		}

		static void OnResize() {
			Gfx.Width = Raylib.GetScreenWidth();
			Gfx.Height = Raylib.GetScreenHeight();
		}
	}
}
