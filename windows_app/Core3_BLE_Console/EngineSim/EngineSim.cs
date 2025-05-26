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

			GUIPanel Panel1 = new GUIPanel();
			Panel1.Position = new Vector2(80, 50);
			Panel1.Size = new Vector2(600, 950);
			Panel1.IsResizable = true;
			Gui.Add(Panel1);

			GUIPanel Panel2 = new GUIPanel();
			Panel2.Position = new Vector2(700, 50);
			Panel2.Size = new Vector2(600, 950);
			Panel2.IsResizable = true;
			Gui.Add(Panel2);

			int ChrIdx = 0;
			float TimeRange = 10;
			GUIChart Chr_RPM = AddChart(Panel1, "RPM", TimeRange, ChrIdx++, Color.Red, 0, 7000);
			Chr_RPM.GetValue = () => Engine.RPM;

			GUIChart Chr_MAP = AddChart(Panel1, "MAP", TimeRange, ChrIdx++, Color.SkyBlue, 0, 230);
			Chr_MAP.GetValue = () => Engine.MAP;

			GUIChart Chr_Ped = AddChart(Panel1, "Pedal / DBW", TimeRange, ChrIdx++, Color.Green, 0, 100);
			Chr_Ped.GetValue = () => Engine.PedalPos;
			Chr_Ped.GetValue2 = () => Engine.DBWPos;
			Chr_Ped.Line2Color = Color.DarkGreen;
			Chr_Ped.UseSecondSamples = true;

			Random Rnd = new Random();

			GUIChart Chr_TLam = AddChart(Panel1, "TgtLam / Lam", TimeRange, ChrIdx++, Color.Orange, 0.6f, 1.2f);
			Chr_TLam.GetValue = () => Engine.TargetLambda;
			Chr_TLam.Line2Color = Color.Red;
			Chr_TLam.GetValue2 = () => Engine.Lambda;
			Chr_TLam.UseSecondSamples = true;

			GUIChart Chr_AirF = AddChart(Panel1, "AirFlow", TimeRange, ChrIdx++, Color.Yellow, 0, 200);
			Chr_AirF.GetValue = () => Engine.AirFlow;

			ChrIdx = 0;
			GUIChart Chr_Tmp = AddChart(Panel2, "CLT / IAT", TimeRange, ChrIdx++, Color.Blue, 0, 110);
			Chr_Tmp.GetValue = () => Engine.CLT;
			Chr_Tmp.Line2Color = Color.SkyBlue;
			Chr_Tmp.GetValue2 = () => Engine.IAT;
			Chr_Tmp.UseSecondSamples = true;

			GUIChart Chr_PW = AddChart(Panel2, "PW", TimeRange, ChrIdx++, Color.White, 0, 20);
			Chr_PW.GetValue = () => Engine.InjPW;

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
			Chr1.Position = new Vector2(10, 10 + (Chr1.Size.Y) * Idx);
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
