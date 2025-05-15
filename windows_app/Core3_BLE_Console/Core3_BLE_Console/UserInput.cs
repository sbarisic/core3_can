using Raylib_cs;

using System;
using System.Collections.Generic;
using System.Linq;
using System.Numerics;
using System.Runtime.Intrinsics.Arm;
using System.Text;
using System.Threading.Tasks;

namespace Core3_BLE_Console {
	delegate bool UserInputOnKeyFunc(KeyboardKey Key); // Return true to stop capturing keys
	delegate void UserInputOnInputCompleteFunc(string Str);

	class UserInput {
		Font DrawFont;
		float FontSpacing = 1;
		int FontSize = 36;

		object Lck = new object();
		List<char> InputBuffer = new List<char>();
		string InputString;

		public UserInput(Font DrawFont, float FontSpacing, int FontSize) {
			this.DrawFont = DrawFont;
			this.FontSpacing = FontSpacing;
			this.FontSize = FontSize;
		}

		bool IsInput = false;
		UserInputOnKeyFunc OnKey;
		UserInputOnInputCompleteFunc OnInputComplete;

		Vector2 MousePosStart;
		int MaxInputLen;

		public bool IsBusy() {
			return IsInput;
		}

		public void BeginInput(Vector2 Position, int MaxInputLen, string InitialText, UserInputOnKeyFunc OnKey, UserInputOnInputCompleteFunc OnInputComplete) {
			lock (Lck) {
				IsInput = true;
				InputBuffer.Clear();

				if (!string.IsNullOrEmpty(InitialText)) {
					InputBuffer.AddRange(InitialText);
					InputString = InitialText;
				}

				this.OnKey = OnKey;
				this.OnInputComplete = OnInputComplete;
				this.MaxInputLen = MaxInputLen;

				MousePosStart = Position;
			}
		}

		public void EndInput() {
			lock (Lck) {
				IsInput = false;

				if (OnInputComplete != null) {
					OnInputComplete(InputString);
					OnInputComplete = null;
				}
			}
		}

		public void StopInput() {
			lock (Lck) {
				IsInput = false;
			}
		}

		public void Draw() {
			if (!IsInput)
				return;

			Color WindowBgColor = new Color(0, 0, 0, 180);
			Vector2 WindowPos = MousePosStart;
			Vector2 WindowSize = new Vector2(MaxInputLen * FontSize + 4, FontSize + 4);

			if (InputBuffer.Count == 0)
				WindowSize = new Vector2(8 + 16, FontSize + 4);
			else
				WindowSize = Raylib.MeasureTextEx(DrawFont, InputString, FontSize, FontSpacing) + new Vector2(16, 0);

			Raylib.DrawRectangleV(WindowPos, WindowSize, WindowBgColor);
			Raylib.DrawRectangleLines((int)WindowPos.X, (int)WindowPos.Y, (int)WindowSize.X, (int)WindowSize.Y, Color.Orange);

			Raylib.DrawTextPro(DrawFont, InputString, WindowPos + new Vector2(8, FontSize / 6), new Vector2(0, 0), 0, FontSize, FontSpacing, Color.White);
		}

		public void Update() {
			lock (Lck) {
				if (!IsInput)
					return;

				int Chr = 0;
				KeyboardKey Keycode = 0;

				do {
					Chr = Raylib.GetCharPressed();

					if (Chr != 0) {
						if ((Chr >= 32) && (Chr <= 125)) {
							if (InputBuffer.Count < MaxInputLen) {
								InputBuffer.Add((char)Chr);
								InputString = new string(InputBuffer.ToArray());
							}
						}
					}
				} while (Chr > 0 && IsInput);

				do {
					Keycode = (KeyboardKey)Raylib.GetKeyPressed();

					if (Keycode == KeyboardKey.Escape) {
						OnKey = null;
						StopInput();
					}

					if (OnKey != null && OnKey(Keycode)) {
						OnKey = null;
						EndInput();
					} else {
						if (Keycode != 0) {
							if (Keycode == KeyboardKey.Enter) {
								InputBuffer.Add('\n');
								InputString = new string(InputBuffer.ToArray());
							} else if (Keycode == KeyboardKey.Backspace) {
								if (InputBuffer.Count > 0) {
									InputBuffer.RemoveAt(InputBuffer.Count - 1);

									if (InputBuffer.Count > 0)
										InputString = new string(InputBuffer.ToArray());
									else
										InputString = "";

								}
							}
						}
					}
				} while (Keycode > 0 && IsInput);
			}
		}
	}
}
