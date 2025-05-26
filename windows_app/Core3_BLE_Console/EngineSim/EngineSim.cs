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
			Chr_RPM.Decimals1 = 0;

			GUIChart Chr_MAP = AddChart(Panel1, "MAP", TimeRange, ChrIdx++, Color.SkyBlue, 0, 230);
			Chr_MAP.GetValue = () => Engine.MAP;
			Chr_MAP.Unit1 = "kPa";
			Chr_MAP.Decimals1 = 2;

			GUIChart Chr_Ped = AddChart(Panel1, "Pedal", TimeRange, ChrIdx++, Color.Green, 0, 100);
			Chr_Ped.GetValue = () => Engine.PedalPos;
			Chr_Ped.Unit1 = "%";
			Chr_Ped.Decimals1 = 1;
			Chr_Ped.Label2 = "DBW";
			Chr_Ped.GetValue2 = () => Engine.DBWPos;
			Chr_Ped.Unit2 = "%";
			Chr_Ped.Decimals2 = 1;
			Chr_Ped.Line2Color = Color.DarkGreen;
			Chr_Ped.UseSecondSamples = true;

			Random Rnd = new Random();

			GUIChart Chr_TLam = AddChart(Panel1, "TgtLam", TimeRange, ChrIdx++, Color.Orange, 0.6f, 1.2f);
			Chr_TLam.GetValue = () => Engine.TargetLambda;
			Chr_TLam.Unit1 = "lambda";
			Chr_TLam.Decimals1 = 3;
			Chr_TLam.Label2 = "Lam";
			Chr_TLam.Unit2 = "lambda";
			Chr_TLam.Decimals2 = 3;
			Chr_TLam.Line2Color = Color.Red;
			Chr_TLam.GetValue2 = () => Engine.Lambda;
			Chr_TLam.UseSecondSamples = true;

			GUIChart Chr_AirF = AddChart(Panel1, "AirFlow", TimeRange, ChrIdx++, Color.Yellow, 0, 200);
			Chr_AirF.GetValue = () => Engine.AirFlow;
			Chr_AirF.Unit1 = "g/s";
			Chr_AirF.Decimals1 = 2;
			Chr_AirF.Label2 = "VE";
			Chr_AirF.Unit2 = "%";
			Chr_AirF.Decimals2 = 1;
			Chr_AirF.Line2Color = Color.Pink;
			Chr_AirF.GetValue2 = () => Engine.VE;
			Chr_AirF.UseSecondSamples = true;

			ChrIdx = 0;
			GUIChart Chr_Tmp = AddChart(Panel2, "CLT", TimeRange, ChrIdx++, Color.Blue, 0, 110);
			Chr_Tmp.GetValue = () => Engine.CLT;
			Chr_Tmp.Unit1 = "degC";
			Chr_Tmp.Decimals1 = 1;
			Chr_Tmp.Label2 = "IAT";
			Chr_Tmp.Unit2 = "degC";
			Chr_Tmp.Decimals2 = 1;
			Chr_Tmp.Line2Color = Color.SkyBlue;
			Chr_Tmp.GetValue2 = () => Engine.IAT;
			Chr_Tmp.UseSecondSamples = true;

			GUIChart Chr_PW = AddChart(Panel2, "PW", TimeRange, ChrIdx++, Color.White, 0, 20);
			Chr_PW.GetValue = () => Engine.InjPW;
			Chr_PW.Unit1 = "ms";
			Chr_PW.Decimals1 = 3;
			Chr_PW.Label2 = "Duty";
			Chr_PW.Unit2 = "%";
			Chr_PW.Decimals2 = 1;
			Chr_PW.Line2Color = Color.Blue;
			Chr_PW.GetValue2 = () => Engine.InjPWDuty;
			Chr_PW.UseSecondSamples = true;

			GUIChart Chr_Exh = AddChart(Panel2, "EMAP", TimeRange, ChrIdx++, Color.Blue, 0, 1100);
			Chr_Exh.GetValue = () => Engine.ExhaustManifoldPressureKPa;
			Chr_Exh.Unit1 = "kPa";
			Chr_Exh.Decimals1 = 2;
			Chr_Exh.Label2 = "TRPM";
			Chr_Exh.Unit2 = "";
			Chr_Exh.Decimals2 = 0;
			Chr_Exh.Line2Color = Color.Red;
			Chr_Exh.GetValue2 = () => Engine.TurboShaftSpeed;
			Chr_Exh.UseSecondSamples = true;

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
				} 
				if (Raylib.IsKeyDown(KeyboardKey.PageDown)) {
					AccelPos -= 30 * Dt;
				} 
				if (Raylib.IsKeyDown(KeyboardKey.KpAdd)) {
					TargetRPM += 2000 * Dt;
				} 
				if (Raylib.IsKeyDown(KeyboardKey.KpSubtract)) {
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
