using ABI.Windows.Foundation;
using Raylib_cs;

using System;
using System.Collections.Generic;
using System.Linq;
using System.Net.NetworkInformation;
using System.Numerics;
using System.Text;
using System.Threading.Tasks;

using Windows.UI.Notifications;

using static System.Net.Mime.MediaTypeNames;

using Font = Raylib_cs.Font;

namespace Core3_BLE_Console.UI {
	class UIVarView : UIElement {
		Color UseBgColor;

		Vector2 NextButtonPosition = new Vector2(20, 20);
		int ButtonSpacing = 42;

		List<BtWatcherVariable> DisplayLines = new List<BtWatcherVariable>();

		public UIVarView(Font DrawFont, float FontSpacing, int FontSize, UserInput UInput) : base(DrawFont, FontSpacing, FontSize, UInput) {
			ElementPosition = new Vector2(1200, 160);
			ElementSize = new Vector2(300, 600);

			BtDataQueue DQ = Bluetooth.GetDataQueue();
			
			foreach (ECUVariable ECUVar in Enum.GetValues<ECUVariable>()) {
				DisplayLines.Add(DQ.GetVariable(ECUVar.ToString(), ECUVar));
			}
		}

		public override bool HandleInput() {
			if (base.HandleInput())
				return true;

			Vector2 MousePos = Program.GetMousePosition();

			if (IsMouseInside(MousePos)) {
				UseBgColor = new Color(0, 0, 0, 120);
				return true;
			}

			UseBgColor = WindowBgColor;
			return false;
		}

		public override void Draw() {
			Raylib.DrawRectanglePro(new Rectangle(ElementPosition, ElementSize), new Vector2(0, 0), 0, UseBgColor);

			Vector2 LineOffset = new Vector2(10, 10);

			for (int i = 0; i < DisplayLines.Count; i++) {
				string Str = string.Format("{0} = {1}", DisplayLines[i].Name.Trim(), DisplayLines[i].ValueFloat);

				Raylib.DrawTextEx(DrawFont, Str, ElementPosition + LineOffset, FontSize, FontSpacing, Color.White);
				LineOffset += new Vector2(0, FontSize / 2 + 5);
			}

			base.Draw();
		}
	}
}
