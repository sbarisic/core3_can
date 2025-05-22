using Raylib_cs;

using System;
using System.Collections.Generic;
using System.Linq;
using System.Net.NetworkInformation;
using System.Numerics;
using System.Text;
using System.Threading.Tasks;

using Windows.UI.Input.Inking;

using static System.Net.Mime.MediaTypeNames;

using Font = Raylib_cs.Font;

namespace Core3_BLE_Console.UI {
	unsafe class UIGraph : UIElement {
		Color UseBgColor;

		Vector2 NextButtonPosition = new Vector2(20, 20);
		int ButtonSpacing = 42;

		public float MaxValue = 5;

		float[] SamplesNormal = new float[128];
		float[] SamplesRaw = new float[128];
		float SampleInterval = 5 / 128.0f;
		float LastSampleTime = 0;

		public BtWatcherVariable Variable;
		public Color GraphColor = Color.White;


		Raylib_cs.Image GraphImage;
		Texture2D GraphTex;

		public UIGraph(SdfFont DrawFont, float FontSpacing, int FontSize, UserInput UInput) : base(DrawFont, FontSpacing, FontSize, UInput) {
			ElementPosition = new Vector2(110, 110);
			ElementSize = new Vector2(800, 150);

			//Variable = Bluetooth.GetDataQueue().GetVariable(0x1);

			GraphImage = Raylib.GenImageColor((int)ElementSize.X, (int)ElementSize.Y, new Color(0, 0, 0, 0));
			GraphTex = Raylib.LoadTextureFromImage(GraphImage);

			AppendSample(0, 0);
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

		float RawMin = 0;
		float RawMax = 0;
		float HeightMax = 0;
		float HeightMin = 0;

		void AppendSample(float Sample, float Raw) {
			Raylib.ImageClearBackground(ref GraphImage, new Color(0, 0, 0, 0));

			fixed (Raylib_cs.Image* GraphImagePtr = &GraphImage) {
				for (int i = 1; i < SamplesNormal.Length; i++) {
					if (i == 1) {
						RawMin = SamplesRaw[0];
						RawMax = SamplesRaw[0];
					}

					SamplesNormal[i - 1] = SamplesNormal[i];
					SamplesRaw[i - 1] = SamplesRaw[i];

					if (SamplesRaw[i] < RawMin)
						RawMin = SamplesRaw[i];

					if (SamplesRaw[i] > RawMax)
						RawMax = SamplesRaw[i];
				}

				SamplesNormal[SamplesNormal.Length - 1] = Sample;
				SamplesRaw[SamplesRaw.Length - 1] = Raw;

				if (Raw < RawMin)
					RawMin = Raw;

				if (Raw > RawMax)
					RawMax = Raw;

				HeightMax = RawMax / MaxValue * ElementSize.Y;
				HeightMin = RawMin / MaxValue * ElementSize.Y;

				for (int i = 0; i < ElementSize.X; i++) {
					float Perc = i / ElementSize.X;
					int SamplesIdx = (int)(Perc * SamplesNormal.Length);

					if (SamplesIdx >= SamplesNormal.Length)
						SamplesIdx = SamplesNormal.Length - 1;

					if (SamplesIdx < 0)
						SamplesIdx = 0;

					float Val = SamplesNormal[SamplesIdx] * ElementSize.Y;

					Raylib.ImageDrawRectangle(GraphImagePtr, i, (int)ElementSize.Y - (int)Val, 1, (int)Val, GraphColor);
				}

				Raylib.ImageDrawLineV(ref GraphImage, new Vector2(0, ElementSize.Y - HeightMax), new Vector2(ElementSize.X, ElementSize.Y - HeightMax), Color.Orange);
				Raylib.ImageDrawLineV(ref GraphImage, new Vector2(0, ElementSize.Y - HeightMin), new Vector2(ElementSize.X, ElementSize.Y - HeightMin), Color.SkyBlue);

				//Raylib.ImageDrawTextEx(ref GraphImage, );

				//Raylib.DrawTextEx(DrawFont, RawMax.ToString(), new Vector2(ElementPosition.X - 50, ElementPosition.Y + (ElementSize.Y - HeightMax) + FontSize / 3), FontSize, FontSpacing, Color.Orange);
				//Raylib.DrawTextEx(DrawFont, RawMin.ToString(), new Vector2(ElementPosition.X - 50, ElementPosition.Y + (ElementSize.Y - HeightMin) + FontSize / 3), FontSize, FontSpacing, Color.SkyBlue);
			}


			//Raylib.ImageFlipVertical(ref GraphImage);
			Raylib.UpdateTexture(GraphTex, GraphImage.Data);
		}

		public override void Draw() {
			if (LastSampleTime <= Variable.Time - SampleInterval) {
				LastSampleTime = Variable.Time;

				float ValueNorm = ((float)Variable.ValueFloat) / MaxValue;
				AppendSample(ValueNorm, Variable.ValueFloat);
			}

			Raylib.DrawRectanglePro(new Rectangle(ElementPosition, ElementSize), new Vector2(0, 0), 0, UseBgColor);
			Raylib.DrawTexture(GraphTex, (int)ElementPosition.X, (int)ElementPosition.Y, Color.White);

			float XOffset = -50;

			Vector2 RawMaxPos = new Vector2(ElementPosition.X + XOffset, ElementPosition.Y + (ElementSize.Y - HeightMax) - (FontSize / 3));
			Vector2 RawMinPos = new Vector2(ElementPosition.X + XOffset, ElementPosition.Y + (ElementSize.Y - HeightMin) - (FontSize / 3));

			Vector2 RawMaxSize = TxtFont.MeasureText(RawMax.ToString(), FontSpacing);
			Vector2 RawMinSize = TxtFont.MeasureText(RawMin.ToString(), FontSpacing);

			//Raylib.DrawTextEx(DrawFont, RawMax.ToString(), RawMaxPos, FontSize, FontSpacing, Color.Orange);
			//Raylib.DrawTextEx(DrawFont, RawMin.ToString(), RawMinPos, FontSize, FontSpacing, Color.SkyBlue);

			TxtFont.DrawTextPro(RawMin.ToString(), new Vector2(ElementPosition.X - 5, RawMinPos.Y), new Vector2(RawMinSize.X, 0), 0,  FontSpacing, Color.SkyBlue);
			TxtFont.DrawTextPro(RawMax.ToString(), new Vector2(ElementPosition.X - 5, RawMaxPos.Y), new Vector2(RawMaxSize.X, 0), 0,  FontSpacing, Color.Orange);
			TxtFont.DrawTextPro(Variable.Name, ElementPosition + new Vector2(5, 5), Vector2.Zero, 0,  FontSpacing, Color.White);


			/*for (int i = 1; i < SamplesNormal.Length; i++) {
				float Y0 = ElementSize.Y - (ElementSize.Y * SamplesNormal[i - 1]);
				float X0 = i - 1;

				float Y1 = ElementSize.Y - (ElementSize.Y * SamplesNormal[i]);
				float X1 = i;

				Raylib.DrawLine((int)(ElementPosition.X + X0), (int)(ElementPosition.Y + Y0), (int)(ElementPosition.X + X1), (int)(ElementPosition.Y + Y1), Color.White);
			}*/

			base.Draw();
		}
	}
}
