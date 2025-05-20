using Raylib_cs;

using System;
using System.Collections.Generic;
using System.Linq;
using System.Numerics;
using System.Text;
using System.Threading.Tasks;

using Windows.UI.StartScreen;

using static System.Net.Mime.MediaTypeNames;

using Font = Raylib_cs.Font;

namespace Core3_BLE_Console.UI {
	class UITable : UIElement {
		bool IsTableDragging = false;
		Vector2 StartDragMousePos;
		Vector2 StartDragWindowPos;


		public byte[] DataBackup;
		public byte[] DataMem;
		public int DataOffset;

		public bool FlipY;
		public int Width;
		public int Height;
		public string TableDesc;
		public string XDesc;
		public string YDesc;
		public string[] XLabels;
		public string[] YLabels;
		public GetTableLabelFunc GetTableLabel;
		public GetTableColorFunc GetTableColor;

		public UITable(Font DrawFont, float FontSpacing, int FontSize, UserInput UInput) : base(DrawFont, FontSpacing, FontSize, UInput) {
			ElementPosition = new Vector2(130, 150);
		}

		string GetTableLabel_Hex(int XX, int YY) {
			int Idx = DataOffset + YY * Width + XX;

			if (Idx >= 0 && Idx < DataMem.Length)
				return DataMem[Idx].ToString("X2");

			return "-";
		}

		float byte_to_correction(byte b) {
			return (75 + ((125 - 75) * (float)(b / 255.0f))) / 100.0f;
		}

		string GetTableLabel_Char(int XX, int YY) {
			int Idx = DataOffset + YY * Width + XX;

			if (Idx >= 0 && Idx < DataMem.Length)
				return ((char)DataMem[Idx]).ToString();

			return "-";
		}

		string GetTableLabel_LTFT(int XX, int YY) {
			int Idx = DataOffset + YY * Width + XX;

			if (Idx >= 0 && Idx < DataMem.Length)
				return MathF.Round(byte_to_correction(DataMem[Idx]), 2).ToString();

			return "-";
		}


		Color GetTableColor_White(int X, int Y) {
			return Color.White;
		}

		Color GetTableColor_LTFT(int X, int Y) {
			byte val = DataMem[Y * Width + X + DataOffset];

			return Utils.LerpColor(Color.SkyBlue, Color.White, Color.Orange, 0.75f, 1.25f, byte_to_correction(val));
		}

		public void SetHexData(byte[] Data) {
			this.DataMem = Data;
		}

		bool WindowHovered = false;

		static Vector2 CellSize = new Vector2(50, 30);

		public override bool HandleInput() {
			if (base.HandleInput())
				return true;

			//Vector2 CellSize = new Vector2(50, 35);
			Vector2 WindowBorderSize = new Vector2(40, 40);
			Vector2 WindowPos = ElementPosition - WindowBorderSize;
			Vector2 WindowSize = new Vector2((Width + 1) * CellSize.X, (Height + 1) * CellSize.Y) + (WindowBorderSize * 2);

			Vector2 MousePos = Raylib.GetMousePosition();

			if (IsTableDragging) {
				Vector2 MouseDelta = MousePos - StartDragMousePos;
				ElementPosition = StartDragWindowPos + MouseDelta;

				if (Raylib.IsMouseButtonReleased(MouseButton.Left) || Raylib.IsMouseButtonUp(MouseButton.Left)) {
					IsTableDragging = false;
				}

				return true;
			}

			if (Utils.IsInside(WindowPos, new Vector2(WindowSize.X, FontSize), MousePos) && !UInput.IsBusy()) {
				WindowHovered = true;

				if (Raylib.IsMouseButtonPressed(MouseButton.Left)) {
					IsTableDragging = true;
					StartDragMousePos = MousePos;
					StartDragWindowPos = ElementPosition;
					return true;
				}
			}

			return false;
		}

		public override void Draw() {
			if (DataMem == null)
				return;

			if (GetTableLabel == null)
				GetTableLabel = GetTableLabel_LTFT; //GetTableLabel_Hex;

			if (GetTableColor == null)
				GetTableColor = GetTableColor_LTFT;

			Vector2 Pos = ElementPosition;

			//Vector2 CellSize = new Vector2(60, 40);
			Vector2 WindowBorderSize = new Vector2(40, 40);

			Color BgColor = Color.White;
			Color AxisBgColor = new Color(196, 196, 196);
			Color OutlineColor = new Color(232, 232, 232);

			Vector2 WindowPos = Pos - WindowBorderSize;
			Vector2 WindowSize = new Vector2((Width + 1) * CellSize.X, (Height + 1) * CellSize.Y) + (WindowBorderSize * 2);


			Raylib.DrawRectangleV(WindowPos, WindowSize, WindowBgColor);
			Raylib.DrawTextEx(DrawFont, TableDesc, Pos - WindowBorderSize + new Vector2(50, 8), FontSize, FontSpacing, WindowHovered ? Color.Orange : Color.White);

			for (int y = 0; y < Height; y++) {
				for (int x = 0; x < Width; x++) {
					DrawCell(y * Width + x, Pos + new Vector2(x * CellSize.X, y * CellSize.Y), CellSize, GetTableColor(x, FlipY ? (Height - y - 1) : y), OutlineColor, GetTableLabel(x, FlipY ? (Height - y - 1) : y));
				}
			}



			OutlineColor = Color.Black;

			for (int x = 0; x < Width; x++) {
				DrawCell(-x - 1, Pos + new Vector2(x * CellSize.X, Height * CellSize.Y), CellSize, AxisBgColor, OutlineColor, XLabels[x]);
			}
			Raylib.DrawTextEx(DrawFont, XDesc, Pos + new Vector2(50, Height * CellSize.Y + 50), FontSize, FontSpacing, Color.White);



			for (int y = 0; y < Height; y++) {
				DrawCell(-y - 1 - Width, Pos + new Vector2(Width * CellSize.X, y * CellSize.Y), CellSize, AxisBgColor, OutlineColor, YLabels[FlipY ? (Height - y - 1) : y]);
			}
			Raylib.DrawTextPro(DrawFont, YDesc, Pos + new Vector2((Width + 1) * CellSize.X + FontSize, 1 * CellSize.Y), new Vector2(0, 0), 90, FontSize, FontSpacing, Color.White);

			ElementSize = WindowSize - WindowBorderSize;
			ElementPosition = Pos;
		}

		void WriteData(int Offset, byte Val) {
			Offset += DataOffset;

			if (Offset < 0 || Offset >= DataMem.Length)
				return;

			DataMem[Offset] = Val;
		}

		byte ReadData(int Offset) {
			Offset += DataOffset;

			if (Offset < 0 || Offset >= DataMem.Length)
				return 0;

			return DataMem[Offset];
		}

		int EditedCellIdx = 0;
		int EditedCellRange = 0;

		void DrawCell(int CellIdx, Vector2 Pos, Vector2 Size, Color BgColor, Color OutlineColor, string Txt) {
			int X = (int)Pos.X;
			int Y = (int)Pos.Y;
			int W = (int)Size.X;
			int H = (int)Size.Y;

			bool IsCellDirty = false;

			if (CellIdx + DataOffset >= 0 && CellIdx + DataOffset < DataMem.Length) {
				if (DataMem[CellIdx + DataOffset] != DataBackup[CellIdx + DataOffset]) {
					IsCellDirty = true;
				}
			}

			if (IsCellDirty) {
				OutlineColor = Color.Blue;
				BgColor = new Color(149, 197, 222);
			}

			if (EditedCellIdx <= CellIdx && (EditedCellIdx + EditedCellRange) >= CellIdx)
				BgColor = Color.Orange;

			Vector2 MousePos = Raylib.GetMousePosition();

			if (Utils.IsInside(Pos, Size, MousePos) && !UInput.IsBusy()) {
				Color HoverColor = Color.Orange;

				if (CellIdx < 0)
					HoverColor = new Color(BgColor.R - 20, BgColor.G - 20, BgColor.B - 20);

				Raylib.DrawRectangle(X, Y, W, H, HoverColor);

				if (IsCellDirty)
					OutlineColor = Color.DarkBlue;
				else
					OutlineColor = Color.Black;

				if (Raylib.IsKeyPressed(KeyboardKey.Delete)) {
					bool CtrlDown = Raylib.IsKeyDown(KeyboardKey.LeftControl);

					if (EditedCellIdx >= 0 && EditedCellRange >= 0) {
						for (int i = 0; i <= EditedCellRange; i++) {
							byte DstByte = 0;

							if (CtrlDown)
								DstByte = DataBackup[EditedCellIdx + i];

							DataMem[EditedCellIdx + i + DataOffset] = DstByte;
						}
					}

				} else if (Raylib.IsMouseButtonPressed(MouseButton.Left) && !UInput.IsBusy()) {
					if (CellIdx >= 0) {
						EditedCellIdx = CellIdx;
						EditedCellRange = 0;

						UInput.BeginInput(
							Pos + Size / 2,
							8,
							"0x" + Txt,
							(Key) => {
								if (Key == KeyboardKey.Enter || Key == KeyboardKey.KpEnter)
									return true;

								return false;
							}, (Str) => {
								//Console.WriteLine(Str);

								try {
									if (Str.StartsWith("0x")) {
										byte B = Convert.FromHexString(Str.Substring(2)).Last();
										WriteData(CellIdx, B);
									} else {
										WriteData(CellIdx, Encoding.ASCII.GetBytes(Str).Last());
									}
								} catch (Exception) {
								}
							});
					}
				} else if (Raylib.IsMouseButtonReleased(MouseButton.Right) && Raylib.IsKeyDown(KeyboardKey.LeftControl) && !UInput.IsBusy()) {
					if (CellIdx >= 0) {
						EditedCellIdx = CellIdx;
						EditedCellRange = 0;

						string DataString = "";

						for (int i = 0; i < 64; i++) {

							byte ByteData = ReadData(CellIdx + i);
							char Chr = (char)ByteData;

							if (ByteData == 0)
								break;

							if (char.IsAsciiLetterOrDigit(Chr) || char.IsPunctuation(Chr) || char.IsWhiteSpace(Chr)) {
								EditedCellRange++;
								DataString += Chr;
							}

						}

						EditedCellRange--;

						UInput.BeginInput(
							Pos + Size / 2,
							DataString.Length,
							DataString,
							(Key) => {
								if (Key == KeyboardKey.Enter || Key == KeyboardKey.KpEnter)
									return true;

								return false;
							}, (Str) => {
								//Console.WriteLine(Str);

								byte[] StrBytes = Encoding.ASCII.GetBytes(Str);

								for (int i = 0; i < StrBytes.Length; i++) {
									WriteData(CellIdx + i, StrBytes[i]);
								}
							});
					}
				} else if (Raylib.IsMouseButtonPressed(MouseButton.Right) && !UInput.IsBusy()) {
					if (CellIdx >= 0) {
						EditedCellIdx = CellIdx;
						EditedCellRange = 0;
					}
				} else if (Raylib.IsMouseButtonReleased(MouseButton.Right) && Raylib.IsKeyDown(KeyboardKey.LeftShift) && !UInput.IsBusy()) {
					if (CellIdx >= 0 && EditedCellIdx >= 0) {
						EditedCellRange = CellIdx - EditedCellIdx;

						if (EditedCellRange < 0) {
							EditedCellIdx = -9999999;
							EditedCellRange = 0;
						} else {
							List<char> Chars = new List<char>();

							for (int i = 0; i <= EditedCellRange; i++) {
								byte ByteData = ReadData(EditedCellIdx + i);
								char Chr = (char)ByteData;

								if (ByteData == 0)
									Chars.Add(' ');

								if (char.IsAsciiLetterOrDigit(Chr) || char.IsPunctuation(Chr) || char.IsWhiteSpace(Chr)) {
									Chars.Add(Chr);
								}

							}

							string DataString = new string(Chars.ToArray());

							UInput.BeginInput(
								Pos + Size / 2,
								DataString.Length,
								DataString,
								(Key) => {
									if (Key == KeyboardKey.Enter || Key == KeyboardKey.KpEnter)
										return true;

									return false;
								}, (Str) => {
									//Console.WriteLine(Str);

									byte[] StrBytes = Encoding.ASCII.GetBytes(Str);

									for (int i = 0; i < StrBytes.Length; i++) {
										WriteData(EditedCellIdx + i, StrBytes[i]);
									}
								});
						}
					}
				} else if (Raylib.IsMouseButtonReleased(MouseButton.Right) && !UInput.IsBusy()) {
					if (CellIdx >= 0 && EditedCellIdx >= 0) {
						EditedCellRange = CellIdx - EditedCellIdx;

						if (EditedCellRange < 0) {
							EditedCellIdx = -9999999;
							EditedCellRange = 0;
						}
					}
				}
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
