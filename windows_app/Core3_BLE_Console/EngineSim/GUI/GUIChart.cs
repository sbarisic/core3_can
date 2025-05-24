using Raylib_cs;
using System.Numerics;
using System;
using System.Diagnostics;

namespace EngineSim {
	delegate void EnumChartSampleFunc(GUIChart Chart, float OffsetNormal, float Val, float Val2);
	delegate float GetValueFunc();

	class GUIChart : GUIElement {
		public float TimeRangeSeconds;
		public float Min;
		public float Max;

		public float[] SamplesArray;

		public bool UseSecondSamples = false;
		public float[] Samples2Array;

		Vector2 DrawPosition;
		Vector2 DrawSize;
		Vector2 LabelAreaSize;
		Vector2 LabelTextOffset;

		public string Label;
		public GetValueFunc GetValue;
		public GetValueFunc GetValue2;

		Stopwatch SampleTimer;
		long LastSampleMs;
		long SamplePeriodMs;

		public Color LineColor = Color.White;
		public Color Line2Color = Color.Gray;
		public float LineThickness = 2;

		public GUIChart(string Name, int Samples, float TimeRangeSeconds, float Min, float Max) {
			Size = new Vector2(100, 150);
			LabelAreaSize = new Vector2(0, 40);
			LabelTextOffset = new Vector2(5, 40);
			SampleTimer = null;

			SamplesArray = new float[Samples];
			Label = Name;
			this.TimeRangeSeconds = TimeRangeSeconds;
			this.Min = Min;
			this.Max = Max;

			Samples2Array = new float[Samples];

			LastSampleMs = 0;
			SamplePeriodMs = (long)((TimeRangeSeconds * 1000) / Samples);
		}

		void RollSampleArray() {
			for (int i = 1; i < SamplesArray.Length; i++) {
				SamplesArray[i - 1] = SamplesArray[i];
				Samples2Array[i - 1] = Samples2Array[i];
			}
		}

		public void AddSample(float Sample, float Sample2) {
			if (SampleTimer == null) {
				SampleTimer = Stopwatch.StartNew();
				LastSampleMs = 0;
			}

			//RollSampleArray();

			if (LastSampleMs + SamplePeriodMs < SampleTimer.ElapsedMilliseconds) {
				LastSampleMs = SampleTimer.ElapsedMilliseconds;

				RollSampleArray();
				SamplesArray[SamplesArray.Length - 1] = Sample;
				Samples2Array[Samples2Array.Length - 1] = Sample2;
			}
		}

		public float CalcTime(float OffsetNormal) {
			return TimeRangeSeconds - (TimeRangeSeconds * OffsetNormal);
		}

		public void EnumerateSamples(EnumChartSampleFunc Func) {
			for (int i = 0; i < SamplesArray.Length; i++) {
				float OffsetNormal = i / (float)(SamplesArray.Length - 1);
				Func(this, OffsetNormal, SamplesArray[i], Samples2Array[i]);
			}
		}

		public override void Update() {
			if (GetValue != null)
				AddSample(GetValue(), GetValue2 != null ? GetValue2() : 0);
		}

		void DrawSample(float ValStart, float ValEnd, float OffsetStart, float OffsetEnd, Color Clr) {
			ValStart = (ValStart - Min) / Max;
			ValEnd = (ValEnd - Min) / Max;

			ValStart = DrawSize.Y - (ValStart * DrawSize.Y);
			ValEnd = DrawSize.Y - (ValEnd * DrawSize.Y);

			Vector2 Start = DrawPosition + new Vector2(DrawSize.X * OffsetStart, ValStart);
			Vector2 End = DrawPosition + new Vector2(DrawSize.X * OffsetEnd, ValEnd);

			Raylib.DrawLineEx(Start, End, LineThickness, Clr);
		}

		public override void Draw(Gfx G) {
			DrawPosition = Position;

			if (Anchor != null) {
				Size.X = Anchor.Size.X - (Position.X * 2);
			}

			DrawSize = Size - LabelAreaSize;

			Raylib.DrawRectangleRec(Rect, Color.White);
			Raylib.DrawRectangleRec(new Rectangle(DrawPosition, DrawSize), Color.Black);

			for (int i = 1; i < SamplesArray.Length; i++) {
				float OffsetStart = (i - 1) / (float)(SamplesArray.Length - 1);
				float OffsetEnd = (i) / (float)(SamplesArray.Length - 1);

				float ValStart = Math.Clamp(SamplesArray[i - 1], Min, Max);
				float ValEnd = Math.Clamp(SamplesArray[i], Min, Max);
				DrawSample(ValStart, ValEnd, OffsetStart, OffsetEnd, LineColor);

				if (UseSecondSamples) {
					float Val2Start = Math.Clamp(Samples2Array[i - 1], Min, Max);
					float Val2End = Math.Clamp(Samples2Array[i], Min, Max);

					DrawSample(Val2Start, Val2End, OffsetStart, OffsetEnd, Line2Color);
				}
			}

			float Val = SamplesArray[SamplesArray.Length - 1];
			float Val2 = Samples2Array[SamplesArray.Length - 1];

			string Line1 = string.Format("{0} {1:0.000}", Label, Val);
			string Line2 = string.Format("Min {0}, Max {1}", Min, Max);

			if (UseSecondSamples) {
				Line1 = string.Format("{0} {1:0.000} / {2:0.000}", Label, Val, Val2);
			}

			G.Font.DrawTextEx(string.Format("{0}\n{1}", Line1, Line2), Position + new Vector2(LabelTextOffset.X, Size.Y - LabelTextOffset.Y), 1, Color.Black);
		}
	}
}
