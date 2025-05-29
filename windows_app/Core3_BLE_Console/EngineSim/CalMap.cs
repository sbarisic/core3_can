using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;
using System.IO;
using System.Runtime.Intrinsics.X86;

namespace EngineSim {
	class CalMap {
		string FileName;

		float[] XAxis;
		float[] YAxis;

		string XLabel;
		string YLabel;

		float[] Values;

		public CalMap(string FilePath) {
			this.FileName = FilePath;
		}

		void ResetValues() {
			Values = new float[XAxis.Length * YAxis.Length];
		}

		public void SetXAxis(string Label, params float[] XAxis) {
			this.XAxis = XAxis;
			XLabel = Label;

			if (YAxis != null)
				ResetValues();
		}

		public void SetYAxis(string Label, params float[] YAxis) {
			this.YAxis = YAxis;
			YLabel = Label;

			if (XAxis != null)
				ResetValues();
		}

		public void ReadFromFile() {
			string[][] CSV = Utils.ParseCSV(File.ReadAllText(FileName));

			float[] XAx = CSV[0].Skip(1).Select(T => float.Parse(T)).ToArray();
			float[] YAx = CSV.Skip(1).Select(L => float.Parse(L[0])).ToArray();

			SetXAxis("X", XAx);
			SetYAxis("Y", YAx);

			for (int y = 0; y < YAx.Length; y++) {
				for (int x = 0; x < XAx.Length; x++) {
					string TxtVal = CSV[1 + y][1 + x];
					float FVal = float.Parse(TxtVal);
					Set(x, y, FVal);
				}
			}
		}

		void FindAxisIndex(float[] Axis, float Val, out int Lower, out int Higher, out float Lerp) {
			Lower = 0;
			Higher = 0;
			Lerp = 0;

			for (int i = 1; i < Axis.Length; i++) {
				float Prev = Axis[i - 1];
				float Cur = Axis[i];

				if (Val < Prev && i == 1)
					return;

				if (i + 1 >= Axis.Length && Val >= Cur) {
					Lower = Axis.Length - 1;
					Higher = Axis.Length - 1;
					Lerp = 0;
					return;
				}

				if (Val >= Prev && Val < Cur) {
					Lower = i - 1;
					Higher = i;

					float MaxVal = Cur - Prev;
					float RealVal = Val - Prev;
					Lerp = RealVal / MaxVal;
					return;
				}
			}
		}

		public void WriteToFile() {

		}

		public void SetRaw(int Idx, float Val) {
			Values[Idx] = Val;
		}

		public void Set(float X, float Y, float Val) {
			SetRaw((int)Y * XAxis.Length + (int)X, Val);
		}

		public float GetRaw(int Idx) {
			return Values[Idx];
		}

		public float GetRaw(int X, int Y) {
			return GetRaw(Y * XAxis.Length + X);
		}

		public float Get(float X, float Y) {
			FindAxisIndex(XAxis, X, out int LowerX, out int HigherX, out float LerpX);
			FindAxisIndex(YAxis, Y, out int LowerY, out int HigherY, out float LerpY);

			float A = GetRaw(LowerX, LowerY); 
			float B = GetRaw(LowerX, HigherY);
			float C = GetRaw(HigherX, HigherY);
			float D = GetRaw(HigherX, LowerY);

			float HighMid = float.Lerp(B, C, LerpX);
			float LowMid = float.Lerp(A, D, LerpX);
			float Mid = float.Lerp(LowMid, HighMid, LerpY);

			return Mid;
		}
	}
}
