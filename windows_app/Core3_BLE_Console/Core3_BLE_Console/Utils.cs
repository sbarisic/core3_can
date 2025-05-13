using System;
using System.Collections.Generic;
using System.Linq;
using System.Numerics;
using System.Text;
using System.Threading.Tasks;

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
	}
}
