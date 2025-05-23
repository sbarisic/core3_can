using Raylib_cs;

using System;
using System.Collections.Generic;
using System.Linq;
using System.Net.NetworkInformation;
using System.Numerics;
using System.Text;
using System.Threading.Tasks;

using static System.Net.Mime.MediaTypeNames;

using Font = Raylib_cs.Font;

namespace Core3_BLE_Console.UI {
	class UIToolbar : UIElement {
		Color UseBgColor;

		Vector2 NextButtonPosition = new Vector2(20, 15);
		int ButtonSpacing = 42;

		public UIToolbar(SdfFont DrawFont, float FontSpacing, int FontSize, UserInput UInput) : base(DrawFont, FontSpacing, FontSize, UInput) {
			RecalculateElementSize();
		}

		void RecalculateElementSize() {
			ElementSize = new Vector2(Program.ProgScreenWidth(), 50);
		}

		public UIButton AddButton(string Text, Action OnClick, Func<UIButton, bool> CheckIsDisabled = null) {
			UIButton Btn1 = new UIButton(TxtFont, FontSpacing, FontSize, UInput);
			Btn1.ButtonText = Text;
			Btn1.ElementPosition = NextButtonPosition;
			Btn1.CalculateSize();
			NextButtonPosition = NextButtonPosition + new Vector2(Btn1.ElementSize.X + ButtonSpacing, 0);
			Btn1.ButtonOnClick = OnClick;
			Btn1.CheckIsDisabled = CheckIsDisabled;

			AddChild(Btn1);
			return Btn1;
		}

		public override bool HandleInput() {
			if (base.HandleInput())
				return true;

			Vector2 MousePos = Program.GetMousePosition();

			if (IsMouseInside(MousePos)) {
				UseBgColor = WindowBgHoverColor;
				return true;
			}

			UseBgColor = WindowBgColor;
			return false;
		}

		public override void Draw() {
			RecalculateElementSize();
			Raylib.DrawRectanglePro(new Rectangle(ElementPosition, ElementSize), new Vector2(0, 0), 0, UseBgColor);

			base.Draw();
		}
	}
}
