using System;
using System.Collections.Generic;
using System.Linq;
using System.Numerics;
using System.Text;
using System.Threading.Tasks;
using Raylib_cs;


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

		public static byte Lerp(byte Min, byte Max, float Val) {
			if (Val < 0)
				Val = 0;

			if (Val > 1)
				Val = 1;

			return (byte)float.Lerp(Min, Max, Val);
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
	}
}
