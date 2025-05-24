using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;


namespace EngineSim {
	class Engine {
		public int RPM = 0;
		public int MAP = 0;
		public int CLT = 0;
		public float PedalPos = 0;
		public float DBWPos = 0;

		public float TurboRequest = 0;
		public float TurboDeliver = 0;
		public float MaxBoost = 80;

		public float TargetLambda;
		public float AirFlow;

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

			float MAF = (Mair * Vd * nvol * ((MAP - (patm / CR)) * RPM) / (R * IAT))/100 ;
			return MAF;
		}

		public void Update(float TimeSec) {
			RPM = TargetRPM + (int)(MathF.Sin(TimeSec * 3) * 10);

			// DBW Follow pedal
			DBWPos = PedalToDBW(PedalPos);

			// Calculate MAP
			float DBWPerc = DBWPos / 100.0f;
			MAP = (int)(20 + (DBWPerc * 80) + (TurboDeliver * MaxBoost));

			// Calculate Target Lambda
			TargetLambda = MAPToTgtLambda(MAP);
			AirFlow = CalculateMAF();

			TurboDeliver = TurboRequest;
		}
	}
}
