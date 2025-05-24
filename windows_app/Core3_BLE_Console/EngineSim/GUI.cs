using System;
using System.Collections.Generic;
using System.Linq;
using System.Numerics;
using System.Text;
using System.Threading.Tasks;

using Raylib_cs;

namespace EngineSim {
	class GUI {
		List<GUIElement> Child = new List<GUIElement>();

		public void Add(GUIElement Element) {
			Child.Add(Element);
		}

		public void Remove(GUIElement Element) {
			if (Child.Contains(Element))
				Child.Remove(Element);
		}

		public void ToTop(GUIElement Element) {
			if (Child.Contains(Element)) {
				Child.Remove(Element);
				Child.Add(Element);
			}
		}

		public static bool UpdateChildren(List<GUIElement> Child, Vector2 PosOffset) {
			Vector2 MousePos = Raylib.GetMousePosition();

			for (int i = Child.Count - 1; i >= 0; i--) {
				GUIElement C = Child[i];

				if (!Utils.IsInside(Utils.AddOffset(C.Rect, PosOffset), MousePos)) {
					if (C.IsMouseInside) {
						C.IsMouseInside = false;
						C.OnMouseLeave();
					}
				}
			}

			for (int i = Child.Count - 1; i >= 0; i--) {
				GUIElement C = Child[i];

				if (Utils.IsInside(Utils.AddOffset(C.Rect, PosOffset), MousePos)) {
					if (!C.IsMouseInside) {
						Child.Remove(C);
						Child.Add(C);
						C.IsMouseInside = true;
						C.OnMouseEnter();
						return true;
					}
				} else {
					if (C.IsMouseInside) {
						C.IsMouseInside = false;
						C.OnMouseLeave();
					}
				}

				if (C.HandleInput())
					break;
			}

			return false;
		}

		public void Update() {
			if (!UpdateChildren(Child, Vector2.Zero)) {
				for (int i = Child.Count - 1; i >= 0; i--) {
					GUIElement C = Child[i];

					if (C.HandleInput())
						break;
				}
			}

			for (int i = Child.Count - 1; i >= 0; i--) {
				GUIElement C = Child[i];
				C.Update();
			}
		}

		public void Draw(Gfx G) {
			for (int i = 0; i < Child.Count; i++) {
				GUIElement C = Child[i];

				C.Draw(G);
			}
		}
	}
}
