using System;
using System.Collections.Generic;
using System.Linq;
using System.Numerics;
using System.Text;
using System.Threading.Tasks;

using Microsoft.VisualBasic.FileIO;

using Raylib_cs;

using TinyCsvParser;


namespace Core3_BLE_Console {
	internal static class Utils {
		public static bool IsInside(Vector2 Pos, Vector2 Size, Vector2 Vertex) {
			if (Vertex.X >= Pos.X && Vertex.X < Pos.X + Size.X) {
				if (Vertex.Y >= Pos.Y && Vertex.Y < Pos.Y + Size.Y) {
					return true;
				}
			}

			return false;
		}

		public static bool IsInside(int X, int Y, int W, int H, int CellIdx, int CellWidth) {
			if (X < 0 || Y < 0)
				return false;

			int IX = CellIdx % CellWidth;
			int IY = (CellIdx - IX) / CellWidth;

			if (W <= 0) {
				X = X + W - 1;
				W = -W + 2;
			}

			if (H <= 0) {
				Y = Y + H - 1;
				H = -H + 2;
			}

			if (IY >= Y && IY < (Y + H)) {
				if (IX >= X && IX < (X + W)) {
					return true;
				}
			}

			return false;
		}

		public static bool IsInside(Vector2 Pos, Vector2 Size, int CellIdx, int CellWidth) {
			return IsInside((int)Pos.X, (int)Pos.Y, (int)Size.X, (int)Size.Y, CellIdx, CellWidth);
		}

		public static byte Lerp(byte Min, byte Max, float Val) {
			if (Val < 0)
				Val = 0;

			if (Val > 1)
				Val = 1;

			return (byte)float.Lerp(Min, Max, Val);
		}

		public static float Lerp(float Min, float Max, float Val) {
			return float.Lerp(Min, Max, Val);
		}

		public static Color LerpColor(Color ClrMin, Color ClrMax, float Val) {
			return new Color(Lerp(ClrMin.R, ClrMax.R, Val), Lerp(ClrMin.G, ClrMax.G, Val), Lerp(ClrMin.B, ClrMax.B, Val));
		}

		public static Color LerpColor(Color ClrMin, Color ClrMid, Color ClrMax, float MinVal, float MaxVal, float Val) {
			float MidVal = MinVal + ((MaxVal - MinVal) / 2);

			if (Val <= MinVal)
				return ClrMin;

			if (Val >= MaxVal)
				return ClrMax;

			if (Val >= MinVal && Val <= MidVal)
				return LerpColor(ClrMin, ClrMid, (Val - MinVal) / (MidVal - MinVal));

			else if (Val >= MidVal && Val <= MaxVal)
				return LerpColor(ClrMid, ClrMax, (Val - MidVal) / (MaxVal - MidVal));

			return Color.Pink;
		}

		public static bool IsDark(Color Clr) {
			int Avg = (Clr.R + Clr.G + Clr.G) / 3;

			return Avg < (255 / 2);
		}

		public static string[][] ParseCSV(string CSV) {
			using (MemoryStream Str = new MemoryStream(Encoding.UTF8.GetBytes(CSV)))
			using (TextFieldParser Parser = new TextFieldParser(Str)) {
				Parser.SetDelimiters("\t");


				List<List<string>> Rows = new List<List<string>>();

				while (Parser.ReadFields() is { } Fields) {
					List<string> Cols = new List<string>();
					Rows.Add(Cols);

					foreach (string F in Fields) {
						//Console.Write("{0}\t", F);
						Cols.Add(F);
					}

					//Console.WriteLine();
				}

				return Rows.Select(Cols => Cols.ToArray()).ToArray();
			}
		}
	}
}
