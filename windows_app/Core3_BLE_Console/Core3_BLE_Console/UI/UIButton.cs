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
	class UIButton : UIElement {
		public Color UseBgColor = new Color(0, 255, 0);
		public Color UseOutlineColor = new Color(255, 0, 0);

		public string ButtonText;
		public Action ButtonOnClick;
		public Func<UIButton, bool> CheckIsDisabled;

		public UIButton(SdfFont DrawFont, float FontSpacing, int FontSize, UserInput UInput) : base(DrawFont, FontSpacing, FontSize, UInput) {
			ElementSize = new Vector2(60, 30);
		}

		bool MouseButtonPressedInside;


		public override bool HandleInput() {
			Vector2 MousePos = Program.GetMousePosition();

			if (CheckIsDisabled != null)
				if (CheckIsDisabled(this)) {
					UseBgColor = new Color(190, 190, 190);
					UseOutlineColor = new Color(200, 200, 200);

					return false;
				}

			if (IsMouseInside(MousePos)) {
				UseBgColor = new Color(0, 0, 0, 180);
				UseOutlineColor = new Color(120, 82, 0);

				if (Raylib.IsMouseButtonPressed(MouseButton.Left)) {
					MouseButtonPressedInside = true;
					return true;
				}

				if (Raylib.IsMouseButtonReleased(MouseButton.Left)) {
					MouseButtonPressedInside = false;
					OnClick();
					return true;
				}

				if (MouseButtonPressedInside)
					UseOutlineColor = Color.Orange;

				return true;
			}

			if (Raylib.IsMouseButtonUp(MouseButton.Left) && MouseButtonPressedInside) {
				MouseButtonPressedInside = false;
			}

			UseBgColor = new Color(0, 0, 0, 160);
			UseOutlineColor = Color.Black;
			return false;
		}

		public virtual void OnClick() {
			if (ButtonOnClick != null)
				ButtonOnClick();
		}

		public void CalculateSize() {
			if (!string.IsNullOrEmpty(ButtonText))
				ElementSize = new Vector2(TxtFont.MeasureText(ButtonText, FontSpacing).X, ElementSize.Y);
		}

		public override void Draw() {
			Vector2 BorderSize = new Vector2(16, 8);
			Color TextColor = Color.White;

			if (MouseButtonPressedInside)
				TextColor = Color.Gray;

			CalculateSize();

			Raylib.DrawRectangleV(ElementPosition - BorderSize, ElementSize + BorderSize * 2, UseBgColor);
			Raylib.DrawRectangleLinesEx(new Rectangle(ElementPosition - BorderSize, ElementSize + BorderSize * 2), 1, UseOutlineColor);

			Color UseTextColor = TextColor;

			if (!Utils.IsDark(UseBgColor))
				UseTextColor = Color.Black;

			if (!string.IsNullOrEmpty(ButtonText))
				TxtFont.DrawTextPro(ButtonText, ElementPosition + new Vector2(0, FontSpacing / 6), Vector2.Zero, 0, FontSpacing, UseTextColor);
		}
	}
}
