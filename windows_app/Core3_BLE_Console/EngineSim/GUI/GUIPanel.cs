using Raylib_cs;
using System.Numerics;
using System;

namespace EngineSim {
	class GUIPanel : GUIElement {
		Color PanelColor = new Color(0, 0, 0, 120);
		Color PanelColorHovered = new Color(0, 0, 0, 140);

		Color ResizePadColor = new Color(0, 0, 0, 30);
		Vector2 ResizePad = new Vector2(10, 10);

		public bool IsResizable = false;


		// Do not edit, automatic variables
		Rectangle Area_ResX;
		Rectangle Area_ResY;

		bool IsDragging = false;
		bool IsResizingX = false;
		bool IsResizingY = false;
		Vector2 StartPanelSize;
		Vector2 StartPanelPos;
		Vector2 StartDragPos;

		List<GUIElement> PanelChild = new List<GUIElement>();

		public void Add(GUIElement Element) {
			PanelChild.Add(Element);
		}

		public void Remove(GUIElement Element) {
			if (PanelChild.Contains(Element))
				PanelChild.Remove(Element);
		}

		public override void Draw(Gfx G) {
			Color PnlClr = PanelColor;

			if (IsMouseInside)
				PnlClr = PanelColorHovered;

			Raylib.DrawRectangleRec(Rect, PnlClr);

			if (IsResizable) {
				Area_ResX = new Rectangle(Position + new Vector2(Size.X - ResizePad.X, 0), new Vector2(ResizePad.X, Size.Y));
				Area_ResY = new Rectangle(Position + new Vector2(0, Size.Y - ResizePad.Y), new Vector2(Size.X, ResizePad.Y));
				Raylib.DrawRectangleRec(Area_ResX, ResizePadColor);
				Raylib.DrawRectangleRec(Area_ResY, ResizePadColor);
			}

			Raylib.BeginMode2D(new Camera2D(Position, Vector2.Zero, 0, 1));
			Raylib.BeginScissorMode((int)Position.X, (int)Position.Y, (int)Size.X, (int)Size.Y);

			for (int i = 0; i < PanelChild.Count; i++) {
				GUIElement C = PanelChild[i];
				C.Draw(G);
			}

			Raylib.EndScissorMode();
			Raylib.EndMode2D();
		}

		public override void Update() {
			for (int i = PanelChild.Count - 1; i >= 0; i--) {
				GUIElement C = PanelChild[i];
				C.Update();
			}
		}

		public override bool HandleInput() {
			Vector2 MousePos = Raylib.GetMousePosition();

			if (Raylib.IsMouseButtonUp(MouseButton.Left)) {
				IsDragging = false;
				IsResizingX = false;
				IsResizingY = false;
			}


			if (IsMouseInside) {
				for (int i = 0; i < PanelChild.Count; i++) {
					GUIElement C = PanelChild[i];

					if (!GUI.UpdateChildren(PanelChild, Position)) {
						if (C.HandleInput())
							return true;
					}
				}

				if (Raylib.IsMouseButtonPressed(MouseButton.Left) && !IsDragging) {
					if (IsResizable) {
						if (Utils.IsInside(Area_ResX, MousePos) && Utils.IsInside(Area_ResY, MousePos)) {
							IsResizingX = true;
							IsResizingY = true;

						} else if (Utils.IsInside(Area_ResX, MousePos)) {
							IsResizingX = true;
							IsResizingY = false;
						} else if (Utils.IsInside(Area_ResY, MousePos)) {
							IsResizingX = false;
							IsResizingY = true;
						}
					}

					if (IsResizingX || IsResizingY) {
						StartPanelSize = Size;
						StartDragPos = MousePos;
					} else {
						IsDragging = true;
						StartPanelPos = Position;
						StartDragPos = MousePos;
					}
				}
			}

			if (IsDragging) {
				Vector2 Delta = MousePos - StartDragPos;
				Position = StartPanelPos + Delta;

				return true;
			} else if (IsResizingX || IsResizingY) {
				Vector2 Delta = MousePos - StartDragPos;

				if (IsResizingX) {
					Size.X = StartPanelSize.X + Delta.X;
				}
				if (IsResizingY) {
					Size.Y = StartPanelSize.Y + Delta.Y;
				}

				return true;
			}

			return false;
		}
	}
}
