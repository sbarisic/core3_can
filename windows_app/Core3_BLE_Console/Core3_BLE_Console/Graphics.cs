using Core3_BLE_Console.UI;

using Raylib_cs;

using System;
using System.Collections.Generic;
using System.Linq;
using System.Net.NetworkInformation;
using System.Numerics;
using System.Reflection.Emit;
using System.Reflection.Metadata.Ecma335;
using System.Text;
using System.Threading.Tasks;

using Windows.Devices.Usb;

using static System.Net.Mime.MediaTypeNames;

using Font = Raylib_cs.Font;

namespace Core3_BLE_Console {
	delegate string GetTableLabelFunc(int X, int Y);
	delegate Color GetTableColorFunc(int X, int Y);

	internal static class Graphics {
		static Font DrawFont;
		static float FontSpacing = 1;
		static int FontSize = 36;

		static UserInput UInput;
		static List<UIElement> UIElements = new List<UIElement>();

		public static void AddUIElement(UIElement El) {
			UIElements.Add(El);
		}

		public static void Init() {
			//DrawFont = Raylib.LoadFontEx("data/fonts/martian_mono.ttf", FontSize, null, 250);
			DrawFont = Raylib.LoadFontEx("data/fonts/mmrtext.ttf", FontSize, null, 250);
			//DrawFont = Raylib.LoadFontEx("data/fonts/Enwallowify_Medium.ttf", FontSize, null, 250);

			UInput = new UserInput(DrawFont, FontSpacing, FontSize);
			
			UITable TestTable = new UITable(DrawFont, FontSpacing, FontSize, UInput);
			//AddUIElement(TestTable);

			UIGraph TestGraph = new UIGraph(DrawFont, FontSpacing, FontSize, UInput);
			AddUIElement(TestGraph);

			UIToolbar Toolbar = new UIToolbar(DrawFont, FontSpacing, FontSize, UInput);
			Toolbar.AddButton("Download Cal", () => { DownloadCalibration(TestTable); }, (Btn) => !Bluetooth.IsConnected());
			Toolbar.AddButton("Erase Cal", () => { EraseCalibration(TestTable); }, (Btn) => !Bluetooth.IsConnected());
			Toolbar.AddButton("Upload Cal", () => { UploadCalibration(TestTable); }, (Btn) => !Bluetooth.IsConnected());
			Toolbar.AddButton("Realtime", () => { RealtimeData(); }, (Btn) => !Bluetooth.IsConnected());

			AddUIElement(Toolbar);


			/*BtDataQueue DQ = Bluetooth.GetDataQueue();
			BtData[] CmdArr = DQ.Commands.Cmd_CalRead(0x0, 256, OnMemReceived).ToArray();

			foreach (BtData Cmd in CmdArr) {
				while (!DQ.TryEnqueueSend(Cmd))
					Thread.Sleep(10);
			}*/

			MemoryStream MS = new MemoryStream(256);
			MS.Write(new byte[] { 0x00 });
			MS.Write(Encoding.UTF8.GetBytes("Hello Memory World!"));
			MS.Write(new byte[256 - MS.Position]);

			OnMemReceived(TestTable, MS.ToArray());
		}

		static void RealtimeData() {
			BtDataQueue DQ = Bluetooth.GetDataQueue();
			BtData[] CmdArr = DQ.Commands.Cmd_VarWatch(0x0).ToArray();

			foreach (BtData Cmd in CmdArr) {
				while (!DQ.TryEnqueueSend(Cmd))
					Thread.Sleep(10);
			}
		}

		static void DownloadCalibration(UITable Tbl) {
			BtDataQueue DQ = Bluetooth.GetDataQueue();
			BtData[] CmdArr = DQ.Commands.Cmd_CalRead(0x0, 256, (Mem) => OnMemReceived(Tbl, Mem)).ToArray();

			foreach (BtData Cmd in CmdArr) {
				while (!DQ.TryEnqueueSend(Cmd))
					Thread.Sleep(10);
			}
		}
		static void EraseCalibration(UITable Tbl) {
			BtDataQueue DQ = Bluetooth.GetDataQueue();

			BtData[] CmdArr = DQ.Commands.Cmd_CalErase(0x0, (uint)Tbl.Data.Length).ToArray();

			foreach (BtData Cmd in CmdArr) {
				while (!DQ.TryEnqueueSend(Cmd))
					Thread.Sleep(10);
			}
		}

		static void UploadCalibration(UITable Tbl) {
			BtDataQueue DQ = Bluetooth.GetDataQueue();
			BtData[] CmdArr = DQ.Commands.Cmd_CalWrite(0x0, (uint)Tbl.Data.Length, Tbl.Data).ToArray();

			foreach (BtData Cmd in CmdArr) {
				while (!DQ.TryEnqueueSend(Cmd))
					Thread.Sleep(10);
			}
		}

		static void OnMemReceived(UITable TestTable, byte[] Mem) {
			Console.WriteLine("Mem: {0}", Mem.Length);

			string[] XLabels = new string[16];

			for (int i = 0; i < 16; i++) {
				XLabels[i] = i.ToString("X2");
			}

			int Height = Mem.Length / 16;
			string[] YLabels = new string[Height];

			for (int i = 0; i < Height; i++) {
				YLabels[i] = (i * 16).ToString("X2");
			}

			byte[] Mem1 = new byte[Mem.Length];
			Array.Copy(Mem, Mem1, Mem1.Length);
			TestTable.Data = Mem1;

			byte[] Mem2 = new byte[Mem.Length];
			Array.Copy(Mem, Mem2, Mem2.Length);

			TestTable.DataBackup = Mem2;
			TestTable.FlipY = false;
			TestTable.Width = XLabels.Length;
			TestTable.Height = YLabels.Length;
			TestTable.TableDesc = "Hex View";
			TestTable.XDesc = "X";
			TestTable.YDesc = "Y";
			TestTable.XLabels = XLabels;
			TestTable.YLabels = YLabels;
		}

		public static void Update() {
			UInput.Update();
		}

		public static void Draw() {
			Raylib.BeginDrawing();
			Raylib.ClearBackground(Color.DarkGreen);

			for (int i = UIElements.Count - 1; i >= 0; i--) {
				if (UIElements[i].HandleInput())
					break;
			}

			for (int i = 0; i < UIElements.Count; i++) {
				UIElements[i].Draw();
			}

			UInput.Draw();

			Raylib.EndDrawing();
		}

		static void DrawHexViewer(byte[] Memory) {
			string[] XLabels = new string[16];

			for (int i = 0; i < 16; i++) {
				XLabels[i] = i.ToString("X2");
			}

			int Height = Memory.Length / 16;
			string[] YLabels = new string[Height];

			for (int i = 0; i < Height; i++) {
				YLabels[i] = (i * 16).ToString("X2");
			}

		}


	}
}
