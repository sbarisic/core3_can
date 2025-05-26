using System;
using System.Collections.Generic;
using System.Linq;
using System.Runtime.Intrinsics.Arm;
using System.Text;
using System.Threading.Tasks;

using Windows.Media.Playback;


namespace EngineSim {
	class Engine {
		Random Rnd = new Random();

		// Static
		float EngineCapacity = 1.364f;
		int Cylinders = 4;

		// Changing
		public int RPM = 0;
		public int MAP = 0;
		public int CLT = 43;
		public int IAT = 21;
		public float PedalPos = 0;
		public float DBWPos = 0;

		public float TurboRequest = 0;
		public float TurboDeliver = 0;
		public float MaxBoost = 130;

		public float Lambda;
		public float TargetLambda;
		public float AirFlow;

		public float VE = 95;
		public float InjPW;
		public float FuelMass;

		public float STFT;

		int TargetRPM = 0;

		public void Pedal(float Pos) {
			if (Pos > 100)
				Pos = 100;

			if (Pos < 0)
				Pos = 0;

			PedalPos = Pos;
		}

		public void SetTargetRPM(float RPM) {
			TargetRPM = (int)RPM;
		}

		float PedalToDBW(float Pedal) {
			if (Pedal == 0)
				return 10;

			float TargetDBW = Pedal / 50.0f;
			if (TargetDBW > 1) {
				TurboRequest = TargetDBW - 1;
				TargetDBW = 1;
			}

			return TargetDBW * 100;
		}

		float MAPToTgtLambda(int MAP) {
			float EnrichPerc = MathF.Max(0, (MAP - 30)) / 70.0f;
			return Utils.Lerp(1.0f, 0.782f, Math.Clamp((MAP - 40) / 60.0f, 0.0f, 1.0f));
		}

		float CalculateMAF() {
			/*
			 MAF [g/s] = Mair ∙ Vd ∙ nvol ∙ (MAP – (patm/CR)) / (R ∙ IAT)

	where:
	Mair is molecular weight of air of 28.9 g.mol-1
	Vd is engine displacement volume in dm3
	nvol is dimensionless engine volumetric efficiency multiplier
	MAP is intake manifold absolute pressure in kPa
	patm is barometric pressure in kPa
	CR is dimensionless engine compression ratio
	R is universal gas constant of 8.314 J.mol-1.K-1
	IAT is intake manifold charge temperature in K
			 */

			float Mair = 28.9f;
			float Vd = 1.364f;
			float nvol = 0.95f;
			float patm = 100;
			float CR = 9.5f;
			float R = 8.314f;
			float IAT = 25 + 274.15f;

			float MAF = (Mair * Vd * nvol * ((MAP - (patm / CR)) * RPM) / (R * IAT)) / 100;
			return MAF;
		}

		float[] LambdaQueue = new float[128];

		public void Update(float TimeSec) {
			RPM = TargetRPM + (int)(MathF.Sin(TimeSec * 3) * 10);

			// DBW Follow pedal
			DBWPos = PedalToDBW(PedalPos);

			// Calculate MAP
			float DBWPerc = DBWPos / 100.0f;
			MAP = (int)(20 + (DBWPerc * 80) + (TurboDeliver * MaxBoost));

			// Calculate IAT

			// Calculate CLT
			if (TimeSec < 95)
				CLT = (int)float.Lerp(CLT, 95, (TimeSec + (MAP / 100.0f) * 10) / 95.0f);
			else
				CLT = 95;

			// Calculate Target Lambda
			TargetLambda = MAPToTgtLambda(MAP);
			AirFlow = CalculateMAF();

			if (AirFlow > 20) {
				TurboDeliver = Utils.Lerp(0, TurboRequest, ((AirFlow) - 20) / Utils.Lerp(120, 50, Math.Clamp(RPM / 4200.0f, 0.0f, 1.0f)));
			} else {
				TurboDeliver = 0;
			}

			if (TurboDeliver < 0)
				TurboDeliver = 0;
			if (TurboDeliver > 1)
				TurboDeliver = 1;


			// Calculate current lambda
			for (int i = 1; i < LambdaQueue.Length; i++) {
				LambdaQueue[i - 1] = LambdaQueue[i];
			}
			LambdaQueue[LambdaQueue.Length - 1] = TargetLambda;

			float Speed = Math.Clamp((AirFlow - 30) / 50.0f, 0, 1);
			float RandomFactor = (Rnd.NextSingle() * 0.05f) - 0.025f;
			Lambda = LambdaQueue[(int)((LambdaQueue.Length - 1) * Speed)] + RandomFactor;

			// Calculate VE
			if (RPM < 4600)
				VE = float.Lerp(75, 95, RPM / 4600);
			else
				VE = float.Lerp(95, 85, (RPM - 4600) / 3000);

			// Calculate Injectors
			float InjSize = 6.52f; // Grams
			float InjOpenTime = 0.84f;
			float AirDensity = (MAP * 1000.0f) / (287.05f * (IAT + 273.15f));
			float GasLambda = 14.7f;

			FuelMass = ((MAP * (EngineCapacity / Cylinders)) / 287.0f) * (1.0f / GasLambda) * (1.0f / (TargetLambda * GasLambda)) * VE;

			InjPW = ((FuelMass / InjSize) * 1000) + InjOpenTime;

		}
	}
}
