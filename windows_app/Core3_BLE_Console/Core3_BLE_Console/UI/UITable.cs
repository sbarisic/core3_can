using Raylib_cs;

using System;
using System.Collections.Generic;
using System.Linq;
using System.Numerics;
using System.Runtime.ConstrainedExecution;
using System.Text;
using System.Threading.Tasks;

using Windows.Media.Playback;
using Windows.UI.StartScreen;

using WinRT;

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

	delegate void GetSelectedFunc(int X, int Y, int Idx, UITableLabel Val);

	class UITable : UIElement {
		UndoBuffer UndoBuffer = new UndoBuffer();

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
		public string DefaultValue = "0";

		public BtWatcherVariable XAxisValue;
		public BtWatcherVariable YAxisValue;

		Vector3 Cell_Hovered;
		Vector2 Cell_Sel = new Vector2(3, 3);
		Vector2 Cell_Selection = new Vector2(4, 5);

		bool Cell_IsSelecting = false;

		Color CursorColor = Color.SkyBlue;
		float CursorBallRadius = 6;
		float CursorLineThick = 2;

		public UITable(SdfFont DrawFont, float FontSpacing, int FontSize, UserInput UInput) : base(DrawFont, FontSpacing, FontSize, UInput) {
			ElementPosition = new Vector2(130, 150);
		}

		bool HasSelection() {
			if (Cell_Sel.X < 0 || Cell_Sel.Y < 0) {
				return false;
			}

			int X = (int)Cell_Sel.X;
			int Y = (int)Cell_Sel.Y;
			int W = (int)Cell_Selection.X;
			int H = (int)Cell_Selection.Y;

			if (W <= 0) {
				X = X + W - 1;
				W = -W + 2;

				Cell_Sel.X = X;
				Cell_Selection.X = W;
			}

			if (H <= 0) {
				Y = Y + H - 1;
				H = -H + 2;

				Cell_Sel.Y = Y;
				Cell_Selection.Y = H;
			}

			return true;
		}

		void EnumerateSelected(GetSelectedFunc GetSelected, Action OnRowBreak = null) {
			if (!HasSelection())
				return;

			int last_y = 0;
			int gets = 0;

			for (int y = 0; y < Height; y++) {
				for (int x = 0; x < Width; x++) {
					if (Utils.IsInside(Cell_Sel, Cell_Selection, y * Width + x, Width)) {
						if (y != last_y && gets > 0) {
							last_y = y;

							if (OnRowBreak != null)
								OnRowBreak();
						}


						GetSelected(x, y, y * Width + x, GetTableLabel(x, y));
						last_y = y;
						gets++;
					}
				}
			}
		}

		public void InterpolateSelection(bool Horizontal) {
			if (!HasSelection())
				return;

			if (Horizontal) {

				for (int y = (int)Cell_Sel.Y; y < (int)(Cell_Sel.Y + Cell_Selection.Y); y++) {
					UITableLabel LblStart = GetTableLabel((int)Cell_Sel.X, y);
					UITableLabel LblEnd = GetTableLabel((int)(Cell_Sel.X + Cell_Selection.X - 1), y);

					for (int x = (int)Cell_Sel.X + 1; x < (int)(Cell_Sel.X + Cell_Selection.X - 1); x++) {
						float fx = (x - Cell_Sel.X) / (Cell_Selection.X - 1);

						float Val = Utils.Lerp(LblStart.Value, LblEnd.Value, fx);

						WriteData(y * Width + x, Val.ToString());
					}

					Console.WriteLine();
				}

			} else {

				for (int x = (int)Cell_Sel.X; x < (int)(Cell_Sel.X + Cell_Selection.X); x++) {
					UITableLabel LblStart = GetTableLabel(x, (int)Cell_Sel.Y);
					UITableLabel LblEnd = GetTableLabel(x, (int)(Cell_Sel.Y + Cell_Selection.Y - 1));

					for (int y = (int)Cell_Sel.Y + 1; y < (int)(Cell_Sel.Y + Cell_Selection.Y - 1); y++) {
						float fy = (y - Cell_Sel.Y) / (Cell_Selection.Y - 1);

						float Val = Utils.Lerp(LblStart.Value, LblEnd.Value, fy);

						WriteData(y * Width + x, Val.ToString());
					}

					Console.WriteLine();
				}

			}
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

			Color Neg = new Color(84, 132, 176);
			Color Pos = new Color(176, 104, 84);

			return Utils.LerpColor(Neg, Color.White, Pos, 0.75f, 1.25f, byte_to_correction(val));
		}

		public void SetHexData(byte[] Data) {
			this.DataMem = Data;
		}

		bool WindowHovered = false;

		static Vector2 CellSize = new Vector2(50, 30);

		bool KeyPressedRepeat(KeyboardKey K) {
			return Raylib.IsKeyPressed(K) || Raylib.IsKeyPressedRepeat(K);
		}

		public override bool HandleInput() {
			if (base.HandleInput())
				return true;

			//Vector2 CellSize = new Vector2(50, 35);
			Vector2 WindowBorderSize = new Vector2(40, 40);
			Vector2 WindowPos = ElementPosition - WindowBorderSize;
			Vector2 WindowSize = new Vector2((Width + 1) * CellSize.X, (Height + 1) * CellSize.Y) + (WindowBorderSize * 2);
			Vector2 MousePos = Program.GetMousePosition();

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

			int TblX = (int)ElementPosition.X;
			int TblY = (int)ElementPosition.Y;
			int TblW = (int)(Width * CellSize.X);
			int TblH = (int)(Height * CellSize.Y);

			if (Raylib.IsKeyPressed(KeyboardKey.Delete) && HasSelection()) {
				bool Success = false;
				BeginUndo();

				EnumerateSelected((X, Y, Idx, Val) => {
					WriteData(Idx, DefaultValue);
					Success = true;
				});

				EndUndo();
				if (Success)
					return true;
			} else if (KeyPressedRepeat(KeyboardKey.KpAdd) && HasSelection()) {
				bool Success = false;
				BeginUndo();

				EnumerateSelected((X, Y, Idx, Val) => {
					WriteData(Idx, (Val.Value + 0.01f).ToString());
					Success = true;
				});

				EndUndo();
				if (Success)
					return true;
			} else if (KeyPressedRepeat(KeyboardKey.KpSubtract) && HasSelection()) {
				bool Success = false;
				BeginUndo();

				EnumerateSelected((X, Y, Idx, Val) => {
					WriteData(Idx, (Val.Value - 0.01f).ToString());
					Success = true;
				});

				EndUndo();
				if (Success)
					return true;
			} else if (KeyPressedRepeat(KeyboardKey.Left) && HasSelection()) {
				Cell_Sel.X -= 1;
				if (Cell_Sel.X < 0)
					Cell_Sel.X = 0;

				return true;
			} else if (KeyPressedRepeat(KeyboardKey.Right) && HasSelection()) {
				Cell_Sel.X += 1;
				if (Cell_Sel.X >= Width)
					Cell_Sel.X = Width - 1;

				return true;
			} else if (KeyPressedRepeat(KeyboardKey.Up) && HasSelection()) {
				Cell_Sel.Y -= 1;
				if (Cell_Sel.Y < 0)
					Cell_Sel.Y = 0;

				return true;
			} else if (KeyPressedRepeat(KeyboardKey.Down) && HasSelection()) {
				Cell_Sel.Y += 1;
				if (Cell_Sel.Y >= Height)
					Cell_Sel.Y = Height - 1;

				return true;
			} else if (Raylib.IsKeyDown(KeyboardKey.LeftControl) && Raylib.IsKeyPressed(KeyboardKey.C) && HasSelection()) {
				StringBuilder CopyBuilder = new StringBuilder();

				EnumerateSelected((X, Y, Idx, Val) => {
					UITableLabel Lbl = GetTableLabel(X, Y);
					CopyBuilder.AppendFormat("{0}\t", Lbl.Label);
				}, () => {
					CopyBuilder.Length--;
					CopyBuilder.AppendLine();
				});

				WindowsClipboard.SetText(CopyBuilder.ToString().Trim());
				return true;
			} else if (Raylib.IsKeyDown(KeyboardKey.LeftControl) && Raylib.IsKeyPressed(KeyboardKey.V) && HasSelection()) {
				string TblTxt = WindowsClipboard.GetText().Trim();
				BeginUndo();

				if (TblTxt.Contains("\n") || TblTxt.Contains("\t")) {
					Console.WriteLine("Table");

					string[][] CSV = Utils.ParseCSV(TblTxt);
					int H = CSV.Length;
					int W = CSV[0].Length;

					Cell_Selection.X = W;
					Cell_Selection.Y = H;

					for (int y = 0; y < H; y++) {
						for (int x = 0; x < W; x++) {
							int xx = x + (int)Cell_Sel.X;
							int yy = y + (int)Cell_Sel.Y;

							WriteData(yy * Width + xx, CSV[y][x]);
						}
					}

				} else if (float.TryParse(TblTxt, out float TblFlt)) {
					WriteData((int)Cell_Sel.Y * Width + (int)Cell_Sel.X, TblTxt);
				}

				EndUndo();
				return true;
			} else if (Raylib.IsKeyDown(KeyboardKey.LeftControl) && Raylib.IsKeyPressed(KeyboardKey.Y) && HasSelection()) {
				PopUndo();
				return true;
			}

			if (Utils.IsInside(new Vector2(TblX, TblY), new Vector2(TblW, TblH), MousePos) && !UInput.IsBusy()) {
				Vector2 TblRelativeCur = MousePos - new Vector2(TblX, TblY);
				Vector2 CellPos = (TblRelativeCur / new Vector2(TblW, TblH)) * new Vector2(Width, Height);

				Cell_Hovered = new Vector3((int)CellPos.X, (int)CellPos.Y, 0);
				Cell_Hovered.Z = (int)Cell_Hovered.Y * Width + (int)Cell_Hovered.X;

				if (Cell_IsSelecting) {
					Cell_Selection = new Vector2((int)Cell_Hovered.X - (int)Cell_Sel.X + 1, (int)Cell_Hovered.Y - (int)Cell_Sel.Y + 1);
				}

				if (Raylib.IsMouseButtonPressed(MouseButton.Left) && !Cell_IsSelecting) {

					Cell_Sel = new Vector2((int)Cell_Hovered.X, (int)Cell_Hovered.Y);
					Cell_Selection = Vector2.Zero;
					Cell_IsSelecting = true;

				} else if ((Raylib.IsMouseButtonReleased(MouseButton.Left) || Raylib.IsMouseButtonUp(MouseButton.Left)) && Cell_IsSelecting) {

					Cell_IsSelecting = false;

				}

				return true;
			} else if (Cell_IsSelecting)
				Cell_IsSelecting = false;

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

			if (XLabels == null || YLabels == null || XLabels.Length == 0 || YLabels.Length == 0)
				return;

			float XPrev = XLabels.Length > 0 ? XLabels[0].Value : 0;
			float XCur = XLabels.Length > 0 ? XLabels[1].Value : 0;
			float XStep = XCur - XPrev;
			float XHyst = XStep / 2;

			float YPrev = YLabels.Length > 0 ? YLabels[0].Value : 0;
			float YCur = YLabels.Length > 0 ? YLabels[1].Value : 0;
			float YStep = YCur - YPrev;
			float YHyst = YStep / 2;

			float CursorX = ElementPosition.X;
			float CursorY = ElementPosition.Y;

			// Highlighted cell by cursor
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
			TxtFont.DrawTextEx(TableDesc, Pos - WindowBorderSize + new Vector2(50, 8), FontSpacing, WindowHovered ? Color.Orange : Color.White);

			// Table
			for (int y = 0; y < Height; y++) {
				for (int x = 0; x < Width; x++) {
					DrawCell(y * Width + x, Pos + new Vector2(x * CellSize.X, y * CellSize.Y), CellSize, GetTableColor(x, FlipY ? (Height - y - 1) : y), OutlineColor, GetTableLabel(x, FlipY ? (Height - y - 1) : y));
				}
			}

			if (DrawSelRect) {
				DrawSelRect = false;

				Raylib.DrawRectangleLinesEx(SelRect, 3, Color.Red);
			}

			OutlineColor = Color.Black;

			// X Axis
			for (int x = 0; x < Width; x++) {
				Color BgClr = AxisBgColor;

				if (HLCellX == x)
					BgClr = Color.SkyBlue;

				DrawCell(-x - 1, Pos + new Vector2(x * CellSize.X, Height * CellSize.Y), CellSize, BgClr, OutlineColor, XLabels[x]);
			}
			TxtFont.DrawTextEx(XDesc, Pos + new Vector2(50, Height * CellSize.Y + FontSize), FontSpacing, Color.White);


			// Y Axis
			for (int y = 0; y < Height; y++) {
				Color BgClr = AxisBgColor;

				if (HLCellY == y)
					BgClr = Color.SkyBlue;

				DrawCell(-y - 1 - Width, Pos + new Vector2(Width * CellSize.X, y * CellSize.Y), CellSize, BgClr, OutlineColor, YLabels[FlipY ? (Height - y - 1) : y]);
			}
			TxtFont.DrawTextPro(YDesc, Pos + new Vector2((Width + 1) * CellSize.X + FontSize, 1 * CellSize.Y), new Vector2(0, 0), 90, FontSpacing, Color.White);

			// Draw cursor
			{
				int X = (int)ElementPosition.X;
				int Y = (int)ElementPosition.Y;
				int W = (int)(Width * CellSize.X);
				int H = (int)(Height * CellSize.Y);
				int CX = (int)CursorX;
				int CY = (int)CursorY;

				Raylib.DrawLineEx(new Vector2(CX, Y), new Vector2(CX, Y + H), CursorLineThick, CursorColor);
				Raylib.DrawLineEx(new Vector2(X, CY), new Vector2(X + W, CY), CursorLineThick, CursorColor);
				Raylib.DrawCircle((int)CursorX, (int)CursorY, CursorBallRadius, CursorColor);
			}

			ElementSize = WindowSize - WindowBorderSize;
			ElementPosition = Pos;
		}

		List<UndoData> CurUndo = new List<UndoData>();

		void BeginUndo() {
			CurUndo.Clear();
		}

		void EndUndo() {
			if (CurUndo.Count == 0)
				return;

			UndoBuffer.Push(CurUndo.ToArray());
			CurUndo.Clear();
		}

		void PopUndo() {
			UndoData[] UndoDat = UndoBuffer.Pop();

			if (UndoDat != null)
				foreach (UndoData Dat in UndoDat) {
					DataMem[Dat.Offset] = Dat.OrigValue;
				}
		}

		void WriteData(int Offset, string In) {
			Offset += DataOffset;

			if (Offset < 0 || Offset >= DataMem.Length)
				return;

			byte Orig = DataMem[Offset];
			DataMem[Offset] = InputToByte(In);
			CurUndo.Add(new UndoData(Offset, DataMem[Offset], Orig));
		}

		byte ReadData(int Offset) {
			Offset += DataOffset;

			if (Offset < 0 || Offset >= DataMem.Length)
				return 0;

			return DataMem[Offset];
		}

		int EditedCellIdx = -9999;
		int EditedCellRange = 0;

		bool DrawSelRect = false;
		Rectangle SelRect;

		void DrawCell(int CellIdx, Vector2 Pos, Vector2 Size, Color BgColor, Color OutlineColor, UITableLabel Txt) {
			int X = (int)Pos.X;
			int Y = (int)Pos.Y;
			int W = (int)Size.X;
			int H = (int)Size.Y;

			bool IsCellDirty = false;
			int OutlineThick = 1;

			if (CellIdx + DataOffset >= 0 && CellIdx + DataOffset < DataMem.Length) {
				if (DataMem[CellIdx + DataOffset] != DataBackup[CellIdx + DataOffset]) {
					IsCellDirty = true;
				}
			}

			if (IsCellDirty) {
				OutlineColor = Color.Blue;
				OutlineThick = 2;
				//BgColor = new Color(149, 197, 222);
			}

			if (EditedCellIdx <= CellIdx && (EditedCellIdx + EditedCellRange) >= CellIdx)
				BgColor = Color.Orange;

			Vector2 MousePos = Program.GetMousePosition();

			if (Utils.IsInside((int)Cell_Sel.X, (int)Cell_Sel.Y, (int)Cell_Selection.X, (int)Cell_Selection.Y, CellIdx, Width)) {
				//BgColor = Color.Purple;
				//OutlineColor = Color.Red;
				//OutlineThick = 1;
			}

			if ((int)Cell_Hovered.Z == CellIdx) {
				BgColor = Color.SkyBlue;
				//OutlineColor = Color.Purple;
				//OutlineThick = 3;
			}

			Raylib.DrawRectangle(X, Y, W, H, BgColor);

			if (CellIdx == (int)((Cell_Sel.Y + Cell_Selection.Y) * Width + (int)(Cell_Sel.X + Cell_Selection.X))) {
				int WW = (int)((Cell_Selection.X) * W);
				int HH = (int)((Cell_Selection.Y) * H);
				int StartX = (int)(X - WW);
				int StartY = (int)(Y - HH);

				if (WW < 0) {
					StartX += WW - (int)(1 * CellSize.X);
					WW = -WW + (int)(2 * CellSize.X);
				}

				if (HH < 0) {
					StartY += HH - (int)(1 * CellSize.Y);
					HH = -HH + (int)(2 * CellSize.Y);
				}

				SelRect = new Rectangle(StartX, StartY, WW, HH);
				DrawSelRect = true;
			}

			if (Utils.IsInside(Pos, Size, MousePos) && !UInput.IsBusy() && false) {


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
			Raylib.DrawRectangleLinesEx(new Rectangle(X, Y, W, H), OutlineThick, OutlineColor);

			Vector2 TxtSz = TxtFont.MeasureText(Txt.Label, FontSpacing);

			TxtFont.DrawTextEx(Txt.Label, new Vector2(X + (W / 2) - (TxtSz.X / 2), Y + H / 6), FontSpacing, Color.Black);
		}

		public void ResetUndoBuffer() {
			if (UndoBuffer != null)
				UndoBuffer.Clear();
		}
	}
}
