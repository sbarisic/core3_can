using Raylib_cs;

using System;
using System.Collections.Generic;
using System.Linq;
using System.Numerics;
using System.Text;
using System.Threading.Tasks;

using static System.Net.Mime.MediaTypeNames;

using Font = Raylib_cs.Font;

namespace Core3_BLE_Console {
	delegate string GetTableLabelFunc(int X, int Y);
	delegate Color GetTableColorFunc(int X, int Y);

	internal static class Graphics {
		static Font DrawFont;
		static float FontSpacing = 1;
		static int FontSize = 36;

		public static void Init() {
			//DrawFont = Raylib.LoadFontEx("data/fonts/martian_mono.ttf", FontSize, null, 250);
			DrawFont = Raylib.LoadFontEx("data/fonts/mmrtext.ttf", FontSize, null, 250);
			//DrawFont = Raylib.LoadFontEx("data/fonts/Enwallowify_Medium.ttf", FontSize, null, 250);
		}

		static Vector2 TableWindowPos = new Vector2(100, 100);

		static bool IsTableDragging = false;
		static Vector2 StartDragMousePos;
		static Vector2 StartDragWindowPos;

		public static void Draw() {
			Raylib.BeginDrawing();
			Raylib.ClearBackground(Color.DarkGreen);

			//DrawCell(new Vector2(100, 100), new Vector2(80, 40), Color.Yellow, "20");

			string[] XLabels = new[] { "1", "2", "3", "4", "5", "6", "7", "8", "9", "10" };
			string[] YLabels = new[] { "1", "2", "3", "4", "5", "6", "7" };

			TableWindowPos = DrawTable(TableWindowPos, true, 10, 7, "Ignition Timing", "RPM Hello World", "MAP Hello World", XLabels, YLabels, (XX, YY) => {
				return (YY * 10 + XX).ToString();
			}, (XX, YY) => {
				int idx = YY * 10 + XX;

				return new Color(255 - idx, 255 - idx, 255 - idx);
			});

			Raylib.EndDrawing();
		}

		static Vector2 DrawTable(Vector2 Pos, bool FlipY, int Width, int Height, string TableDesc, string XDesc, string YDesc, string[] XLabels, string[] YLabels, GetTableLabelFunc GetTableLabel, GetTableColorFunc GetTableColor) {
			Vector2 CellSize = new Vector2(60, 40);
			Vector2 WindowBorderSize = new Vector2(40, 40);

			Color BgColor = Color.White;
			Color AxisBgColor = new Color(196, 196, 196);
			Color WindowBgColor = new Color(0, 0, 0, 80);
			Color OutlineColor = new Color(232, 232, 232);


			Vector2 MousePos = Raylib.GetMousePosition();

			if (IsTableDragging) {
				Vector2 MouseDelta = MousePos - StartDragMousePos;
				Pos = StartDragWindowPos + MouseDelta;

				if (Raylib.IsMouseButtonReleased(MouseButton.Left) || Raylib.IsMouseButtonReleased(MouseButton.Left)) {
					IsTableDragging = false;
				}
			}

			Vector2 WindowPos = Pos - WindowBorderSize;
			Vector2 WindowSize = new Vector2((Width + 1) * CellSize.X, (Height + 1) * CellSize.Y) + (WindowBorderSize * 2);
			bool WindowHovered = false;

			if (Utils.IsInside(WindowPos, new Vector2(WindowSize.X, FontSize), MousePos)) {
				WindowHovered = true;

				if (Raylib.IsMouseButtonPressed(MouseButton.Left)) {
					IsTableDragging = true;
					StartDragMousePos = MousePos;
					StartDragWindowPos = Pos;
				}
			}

			Raylib.DrawRectangleV(WindowPos, WindowSize, WindowBgColor);
			Raylib.DrawTextEx(DrawFont, TableDesc, Pos - WindowBorderSize + new Vector2(50, 8), FontSize, FontSpacing, WindowHovered ? Color.Orange : Color.White);

			for (int y = 0; y < Height; y++) {
				for (int x = 0; x < Width; x++) {
					DrawCell(Pos + new Vector2(x * CellSize.X, y * CellSize.Y), CellSize, GetTableColor(x, FlipY ? (Height - y - 1) : y), OutlineColor, GetTableLabel(x, FlipY ? (Height - y - 1) : y));
				}
			}



			OutlineColor = Color.Black;

			for (int x = 0; x < Width; x++) {
				DrawCell(Pos + new Vector2(x * CellSize.X, Height * CellSize.Y), CellSize, AxisBgColor, OutlineColor, XLabels[x]);
			}
			Raylib.DrawTextEx(DrawFont, XDesc, Pos + new Vector2(50, Height * CellSize.Y + 50), FontSize, FontSpacing, Color.White);



			for (int y = 0; y < Height; y++) {
				DrawCell(Pos + new Vector2(Width * CellSize.X, y * CellSize.Y), CellSize, AxisBgColor, OutlineColor, YLabels[FlipY ? (Height - y - 1) : y]);
			}
			Raylib.DrawTextPro(DrawFont, YDesc, Pos + new Vector2((Width + 1) * CellSize.X + FontSize, 1 * CellSize.Y), new Vector2(0, 0), 90, FontSize, FontSpacing, Color.White);

			return Pos;
		}

		static void DrawCell(Vector2 Pos, Vector2 Size, Color BgColor, Color OutlineColor, string Txt) {
			int X = (int)Pos.X;
			int Y = (int)Pos.Y;
			int W = (int)Size.X;
			int H = (int)Size.Y;

			Vector2 MousePos = Raylib.GetMousePosition();

			if (Utils.IsInside(Pos, Size, MousePos)) {
				Raylib.DrawRectangle(X, Y, W, H, Color.Orange);
				OutlineColor = Color.Black;
			} else {
				Raylib.DrawRectangle(X, Y, W, H, BgColor);
			}


			//Raylib.DrawRectangleLines(X, Y, W, H, Color.Black);
			int LineThick = 1;
			Raylib.DrawRectangleLinesEx(new Rectangle(X, Y, W, H), LineThick, OutlineColor);

			Vector2 TxtSz = Raylib.MeasureTextEx(DrawFont, Txt, FontSize, FontSpacing);

			Raylib.DrawTextEx(DrawFont, Txt, new Vector2(X + (W / 2) - (TxtSz.X / 2), Y + H / 6), FontSize, FontSpacing, Color.Black);
		}
	}
}
