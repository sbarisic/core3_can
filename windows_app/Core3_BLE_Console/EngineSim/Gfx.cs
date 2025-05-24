using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;

using Raylib_cs;

namespace EngineSim {
	class Gfx {
		public int Width;
		public int Height;

		public int FontSize;
		public SdfFont Font;

		public Gfx() {
			FontSize = 18;
			Font = new SdfFont("data/fonts/anonymous_pro_bold.ttf", FontSize);
		}

		public void Update() {

		}

		public void BeginDraw() {
			Raylib.BeginDrawing();
			Raylib.ClearBackground(Color.White);
		}

		public void EndDraw() {
			Raylib.EndDrawing();
		}
	}
}
