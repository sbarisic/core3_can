using Raylib_cs;
using System.Numerics;
using System;

namespace EngineSim {
	class GUIElement {
		public Vector2 Position;
		public Vector2 Size;

		public bool IsMouseInside;
		public GUIElement Anchor;

		public Rectangle Rect {
			get {
				return new Rectangle(Position, Size);
			}
		}

		public virtual void OnMouseEnter() {
		}

		public virtual void OnMouseLeave() {
		}

		public virtual bool HandleInput() {
			return false;
		}

		public virtual void Update() {
		}

		public virtual void Draw(Gfx G) {
		}
	}
}
