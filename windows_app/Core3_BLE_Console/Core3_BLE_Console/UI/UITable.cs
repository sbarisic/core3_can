using Raylib_cs;

using System;
using System.Collections.Generic;
using System.Linq;
using System.Numerics;
using System.Runtime.ConstrainedExecution;
using System.Text;
using System.Threading.Tasks;

using Windows.ApplicationModel.DataTransfer;
using Windows.Media.Playback;
using Windows.UI.StartScreen;

using static System.Net.Mime.MediaTypeNames;

using Font = Raylib_cs.Font;

namespace Core3_BLE_Console.UI {
	delegate byte InputToByteFunc(string Input);
	delegate UITableLabel ByteToInputFunc(byte B);

	class UITableLabel {
		public string Label;
		public float Value;

		public UITableLabel(string Lbl, float Val) {
			this.Label = Lbl;
			this.Value = Val;
		}
	}

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
		public UITableLabel[] XLabels;
		public UITableLabel[] YLabels;
		public GetTableLabelFunc GetTableLabel;
		public GetTableColorFunc GetTableColor;

		public InputToByteFunc InputToByte;
		public ByteToInputFunc ByteToInput;

		public BtWatcherVariable XAxisValue;
		public BtWatcherVariable YAxisValue;

		public UITable(Font DrawFont, float FontSpacing, int FontSize, UserInput UInput) : base(DrawFont, FontSpacing, FontSize, UInput) {
			ElementPosition = new Vector2(130, 150);
		}

		string GetTableLabel_Hex(int XX, int YY) {
			int Idx = DataOffset + YY * Width + XX;

			if (Idx >= 0 && Idx < DataMem.Length)
				return DataMem[Idx].ToString("X2");

			return "-";
		}

		public static float byte_to_correction(byte b) {
			return (75 + ((125 - 75) * (float)(b / 255.0f))) / 100.0f;
		}

		public static byte correction_to_byte(float cor) {
			if (cor < 0.75)
				return 0;

			if (cor > 1.25)
				return 255;

			return (byte)((cor - 0.75) / (1.25 - 0.75) * 255);
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

		UITableLabel GetTableLabel_FromDelegate(int X, int Y) {
			int Idx = DataOffset + Y * Width + X;

			if (Idx >= 0 && Idx < DataMem.Length) {
				return ByteToInput(DataMem[Idx]);
			}

			return new UITableLabel("-", 0);
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
				GetTableLabel = GetTableLabel_FromDelegate; //GetTableLabel_Hex;

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

			float XVal = XAxisValue.ValueFloat;
			float YVal = YAxisValue.ValueFloat;

			if (XLabels == null || YLabels == null)
				return;

			float XPrev = XLabels[0].Value;
			float XCur = XLabels[1].Value;
			float XStep = XCur - XPrev;
			float XHyst = XStep / 2;

			float YPrev = YLabels[0].Value;
			float YCur = YLabels[1].Value;
			float YStep = YCur - YPrev;
			float YHyst = YStep / 2;

			float CursorX = ElementPosition.X;
			float CursorY = ElementPosition.Y;

			int HLCellX = 0;
			int HLCellY = 0;

			// X Axis offset
			for (int i = 0; i < Width; i++) {
				float Val = XLabels[i].Value;
				float Lo = Val - XHyst;
				float Hi = Val + XHyst;

				if (XVal >= Lo && XVal < Hi) {
					float XFactor = ((XVal - Lo) / XStep) - 0.5f;
					CursorX += (CellSize.X * i) + (CellSize.X * XFactor) + (CellSize.X / 2);
					HLCellX = i;
					break;
				}
			}

			// Y Axis offset
			for (int i = 0; i < Height; i++) {
				float Val = YLabels[i].Value;
				float Lo = Val - YHyst;
				float Hi = Val + YHyst;

				if (YVal >= Lo && YVal < Hi) {
					float YFactor = ((YVal - Lo) / YStep) - 0.5f;
					CursorY += (CellSize.Y * i) + (CellSize.Y * YFactor) + (CellSize.Y / 2);
					HLCellY = i;
					break;
				}
			}

			Raylib.DrawRectangleV(WindowPos, WindowSize, WindowBgColor);
			Raylib.DrawTextEx(DrawFont, TableDesc, Pos - WindowBorderSize + new Vector2(50, 8), FontSize, FontSpacing, WindowHovered ? Color.Orange : Color.White);

			// Table
			for (int y = 0; y < Height; y++) {
				for (int x = 0; x < Width; x++) {
					DrawCell(y * Width + x, Pos + new Vector2(x * CellSize.X, y * CellSize.Y), CellSize, GetTableColor(x, FlipY ? (Height - y - 1) : y), OutlineColor, GetTableLabel(x, FlipY ? (Height - y - 1) : y));
				}
			}



			OutlineColor = Color.Black;

			// X Axis
			for (int x = 0; x < Width; x++) {
				Color BgClr = AxisBgColor;

				if (HLCellX == x)
					BgClr = Color.SkyBlue;

				DrawCell(-x - 1, Pos + new Vector2(x * CellSize.X, Height * CellSize.Y), CellSize, BgClr, OutlineColor, XLabels[x]);
			}
			Raylib.DrawTextEx(DrawFont, XDesc, Pos + new Vector2(50, Height * CellSize.Y + 50), FontSize, FontSpacing, Color.White);


			// Y Axis
			for (int y = 0; y < Height; y++) {
				Color BgClr = AxisBgColor;

				if (HLCellY == y)
					BgClr = Color.SkyBlue;

				DrawCell(-y - 1 - Width, Pos + new Vector2(Width * CellSize.X, y * CellSize.Y), CellSize, BgClr, OutlineColor, YLabels[FlipY ? (Height - y - 1) : y]);
			}
			Raylib.DrawTextPro(DrawFont, YDesc, Pos + new Vector2((Width + 1) * CellSize.X + FontSize, 1 * CellSize.Y), new Vector2(0, 0), 90, FontSize, FontSpacing, Color.White);

			// Draw cursor
			{
				int X = (int)ElementPosition.X;
				int Y = (int)ElementPosition.Y;
				int W = (int)(Width * CellSize.X);
				int H = (int)(Height * CellSize.Y);
				int CX = (int)CursorX;
				int CY = (int)CursorY;

				Raylib.DrawLine(CX, Y, CX, Y + H, Color.Red);
				Raylib.DrawLine(X, CY, X + W, CY, Color.Red);

				Raylib.DrawCircle((int)CursorX, (int)CursorY, 5, Color.Red);
			}

			ElementSize = WindowSize - WindowBorderSize;
			ElementPosition = Pos;
		}

		void WriteData(int Offset, string In) {
			Offset += DataOffset;

			if (Offset < 0 || Offset >= DataMem.Length)
				return;

			DataMem[Offset] = InputToByte(In);
		}

		byte ReadData(int Offset) {
			Offset += DataOffset;

			if (Offset < 0 || Offset >= DataMem.Length)
				return 0;

			return DataMem[Offset];
		}

		int EditedCellIdx = 0;
		int EditedCellRange = 0;

		void DrawCell(int CellIdx, Vector2 Pos, Vector2 Size, Color BgColor, Color OutlineColor, UITableLabel Txt) {
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

				if (Raylib.IsKeyDown(KeyboardKey.LeftControl) && Raylib.IsKeyPressed(KeyboardKey.C)) {
					Console.WriteLine("Copy {0}", Txt.Label);

					WindowsClipboard.SetText(Txt.Label);


				} else if (Raylib.IsKeyDown(KeyboardKey.LeftControl) && Raylib.IsKeyPressed(KeyboardKey.V)) {
					string ClipStr = WindowsClipboard.GetText();

					if (!string.IsNullOrEmpty(ClipStr)) {

						Console.WriteLine("Paste {0}", ClipStr);

						try {
							WriteData(CellIdx, ClipStr);
						} catch (Exception E) {
							Console.WriteLine("Fail: {0}", E.Message);
						}

					}

				} else if (Raylib.IsKeyPressed(KeyboardKey.Delete)) {
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
							Txt.Label,
							(Key) => {
								if (Key == KeyboardKey.Enter || Key == KeyboardKey.KpEnter)
									return true;

								return false;
							}, (Str) => {
								//Console.WriteLine(Str);

								try {
									WriteData(CellIdx, Str);
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
									//WriteData(CellIdx + i, StrBytes[i]);
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

									//byte[] StrBytes = Encoding.ASCII.GetBytes(Str);

									//for (int i = 0; i < StrBytes.Length; i++) {
									//	WriteData(EditedCellIdx + i, Str);
									//}
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

			Vector2 TxtSz = Raylib.MeasureTextEx(DrawFont, Txt.Label, FontSize, FontSpacing);

			Raylib.DrawTextEx(DrawFont, Txt.Label, new Vector2(X + (W / 2) - (TxtSz.X / 2), Y + H / 6), FontSize, FontSpacing, Color.Black);
		}
	}
}
