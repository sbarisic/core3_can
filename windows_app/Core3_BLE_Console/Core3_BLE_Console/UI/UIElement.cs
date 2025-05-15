using Raylib_cs;

using System;
using System.Collections.Generic;
using System.Linq;
using System.Numerics;
using System.Text;
using System.Threading.Tasks;

using Windows.Security.EnterpriseData;

using static System.Net.Mime.MediaTypeNames;

using Font = Raylib_cs.Font;

namespace Core3_BLE_Console.UI {
	abstract class UIElement {
		protected List<UIElement> Children = new List<UIElement>();

		protected Font DrawFont;
		protected float FontSpacing = 1;
		protected int FontSize = 36;
		protected UserInput UInput;

		protected readonly Color WindowBgColor = new Color(0, 0, 0, 80);

		public Vector2 ElementPosition;
		public Vector2 ElementSize;

		public UIElement(Font DrawFont, float FontSpacing, int FontSize, UserInput UInput) {
			this.DrawFont = DrawFont;
			this.FontSpacing = FontSpacing;
			this.FontSize = FontSize;
			this.UInput = UInput;
		}

		public virtual void AddChild(UIElement Element) {
			Children.Add(Element);
		}

		public virtual bool HandleInput() {
			for (int i = Children.Count - 1; i >= 0; i--) {
				if (Children[i].HandleInput())
					return true;
			}

			return false;
		}

		public virtual bool IsMouseInside(Vector2 MousePos) {
			return Utils.IsInside(ElementPosition, ElementSize, MousePos);
		}

		public virtual void Draw() {
			for (int i = 0; i < Children.Count; i++) {
				Children[i].Draw();
			}
		}
	}
}
