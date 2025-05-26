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
		// Constants

		const float SpecificGasConstant = 287.058f; // J/(Kg*K)


		const float BoreMm = 72.5f; // meter
		const float StrokeMm = 82.6f; // meter
		const float PistonRadiusMm = (BoreMm / 2.0f);

		const float CompressionRatio = 9.5f;

		const float NumOfCylinders = 4;
		const float PowerStrokesPerRPM = 0.5f;
		static float DisplacementL = (float)((Math.PI * (PistonRadiusMm * PistonRadiusMm) * StrokeMm * NumOfCylinders) / 1000000.0f);

		const float ThrottleAreaMm2 = 1249.0f;
		const float IntakeManifoldDisplacementL = 4.5f; // L
		const float ThrottleMaxFlowAirMass = 80.0f; // g @ 100 kPa
		public float AirMassInManifold = 0; // g/manifold
		float AirMassAddedPercent = 0; // %

		const float ExhaustManifoldDisplacementL = 2.5f; // L
		public float ExhaustManifoldTempC = 0.0f; // C
		public float ExhaustManifoldPressureKPa = 100.0f; // kPa
		float ExMassInExhaust = 0;

		public int AmbientTemp = 21; // C
		float Baro = 100.0f; // kPa
		 float IntakeEfficiency = 0.99f;

		// Turbocharger
		public float WastegateDC = 100.0f;
		public float TurboShaftSpeed = 0;

		public int RPM = 0;
		public float MAP = 0;
		public float CLT = 43;
		public float IAT = 0;
		public float PedalPos = 0;
		public float DBWPos = 0;

		public float ICDiameter = 64.0f; // mm
		public float ICPressure = 0; // kPa
		public float ICTemp = 0; // C

		public float TurboRequest = 0;
		public float TurboDeliver = 0;
		public float MaxBoost = 130;

		public float Lambda;
		public float TargetLambda;
		public float AirFlow;

		public float VE = 95;
		public float InjPW;
		public float InjPWDuty;
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
				return 1.5f;

			float TargetDBW = Pedal / 50.0f;
			if (TargetDBW > 1) {
				TurboRequest = TargetDBW - 1;
				TargetDBW = 1;
			}

			return TargetDBW * 100;
		}

		float MAPToTgtLambda(float MAP) {
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

		float CalcAirMassG(float DensityKgM3, float DisplacementL) {
			return DensityKgM3 * LToM3(DisplacementL) * 1000.0f;
		}

		float CalcAirDensity(float AirMassG, float DisplacementL) {
			return AirMassG / (LToM3(DisplacementL) * 1000.0f);
		}

		float CalcDensityKgM3(float PressureKPa, float TempC) {
			return KPaToPascal(PressureKPa) / (SpecificGasConstant * ToKelvin(TempC));
		}

		float CalcPressureKPa(float DensityKgM3, float TempC) {
			return (DensityKgM3 * (SpecificGasConstant * ToKelvin(TempC))) / 1000.0f;
		}

		float CalcTemperatureC(float DensityKgM3, float PressureKPa) {
			float Something = (1.0f / (DensityKgM3 / KPaToPascal(PressureKPa))) / SpecificGasConstant;

			return ToCelsius(Something);
		}

		float CalcThrottleFlow(float Perc) {
			Perc = Perc * 100;

			float ret = (-0.0105676f * (Perc * Perc) + 2.05694f * (Perc) + 0.0986014f) / 100.0f;

			if (ret < 0)
				return 0;

			if (ret > 1)
				return 1;

			return ret;
		}

		float CalcWastegateFlow(float WGDC) {
			float FlowPerc = 1 - WGDC;

			return 1 - Math.Clamp(-0.445221f * (FlowPerc * FlowPerc) + 1.45886f * FlowPerc - 0.00769231f, 0, 1);
		}

		const float MaxTurboSpeed = 250000;

		float CalcTurboShaftSpeed(float UpperExAirMassLimit, float ExAirMass) {
			float ShaftSpeed = float.Lerp(0, 250000, Math.Clamp(ExAirMass / UpperExAirMassLimit, 0, 1));
			return ShaftSpeed;
		}

		float CalcTurboPressureRatio(float ShaftSpeed) {
			float SpeedOffset = 30000;
			return float.Lerp(1, 5.0f, Math.Clamp((ShaftSpeed - SpeedOffset) / (MaxTurboSpeed - SpeedOffset), 0, 1));
		}

		float CalcTurboEfficiency() {
			float PressRat = CalcTurboPressureRatio(TurboShaftSpeed);

			float PressRatEff = 1.0f;

			float LowerPressRat = 2.0f;
			float HigherPressRat = 2.5f;
			float TurboShaftSpeedMax = 230000;
			float TurboShaftSpeedHyst = 25000;

			if (PressRat <= LowerPressRat)
				PressRatEff = float.Lerp(0.0f, 1.0f, PressRat / LowerPressRat);
			else if (PressRat > HigherPressRat)
				PressRatEff = float.Lerp(1.0f, 0.0f, (PressRat - HigherPressRat) / HigherPressRat);

			float ShaftSpeedEff = 1.0f;
			if (TurboShaftSpeed > TurboShaftSpeedMax)
				ShaftSpeedEff = float.Lerp(1.0f, 0.0f, Math.Clamp((TurboShaftSpeed - TurboShaftSpeedMax) / TurboShaftSpeedHyst, 0, 1));

			return Math.Clamp(PressRatEff * ShaftSpeedEff, 0.6f, 0.86f);
		}


		float LastTimeSec = 0;

		public void Update(float TimeSec) {
			float Dt = TimeSec - LastTimeSec;
			LastTimeSec = TimeSec;

			if (Dt > 0.5f)
				return;

			//============================ Engine data

			RPM = TargetRPM + (int)(MathF.Sin(TimeSec * 3) * 10);
			float RoundsPerSecond = RPM / 60.0f;
			float IntakeCyclesPerSecond = RoundsPerSecond * (NumOfCylinders * PowerStrokesPerRPM);

			//======================= Intake after turbo

			float PresRat = CalcTurboPressureRatio(TurboShaftSpeed);
			ICPressure = Baro * PresRat;

			float TurboEff = CalcTurboEfficiency();

			float NewICTemp = CalcTemperatureC(CalcDensityKgM3(Baro, AmbientTemp), ICPressure);
			NewICTemp = float.Lerp(NewICTemp, AmbientTemp, TurboEff); // IC Efficiency
			ICTemp = Utils.Weighted(ICTemp, NewICTemp, 1 - AirMassAddedPercent, AirMassAddedPercent);
			ICTemp = Utils.Weighted(ICTemp, AmbientTemp, 1 - AirMassAddedPercent, AirMassAddedPercent);

			Console.WriteLine("ICTemp = {0}", ICTemp);

			float ICAirDensity = CalcDensityKgM3(ICPressure, ICTemp); // Kg/m3


			//====================== DBW

			DBWPos = PedalToDBW(PedalPos);
			float DBWPerc = DBWPos / 100.0f;
			//float ThrottleAreaCurrent = (ThrottleAreaMm2 * DBWPos) / 100.0f; // mm2

			if (PedalPos > 50.0f) {
				WastegateDC = Math.Clamp(float.Lerp(1, 99, (PedalPos - 50) / 50), 1, 99);
			} else {
				WastegateDC = 5;
			}


			// Scale throttle max flow grams based on intake pressure (kPa)
			float ScaledThrottleMaxFlow = (ThrottleMaxFlowAirMass * (ICPressure / 100.0f)); // g
			float ThrottleAirMass = ScaledThrottleMaxFlow * CalcThrottleFlow(DBWPerc); // g

			//============================= Throttle air mass
			// Add throttle flow air mass to manifold air mass
			float MaxAirMassInManifold = CalcAirMassG(ICAirDensity, IntakeManifoldDisplacementL); // g/manifold;
			AirMassAddedPercent = (ThrottleAirMass * Dt) / MaxAirMassInManifold;
			float OldAirMassInManifold = AirMassInManifold;
			AirMassInManifold = OldAirMassInManifold + ThrottleAirMass * Dt;
			float AirMassAdded = (MaxAirMassInManifold - OldAirMassInManifold);

			if (AirMassInManifold > MaxAirMassInManifold) {
				AirMassInManifold = MaxAirMassInManifold;
				AirMassAdded = (MaxAirMassInManifold - OldAirMassInManifold);
				AirMassAddedPercent = AirMassAdded / MaxAirMassInManifold;
			}

			//============================ Manifold density/pressure
			float ManifoldDensity = CalcAirDensity(AirMassInManifold, IntakeManifoldDisplacementL);
			float ManifoldPressure = CalcPressureKPa(ManifoldDensity, ICTemp);

			//============================== MAP
			MAP = (int)ManifoldPressure;

			//=============================== IAT
			IAT = Utils.Weighted(IAT, ICTemp, 1 - (AirMassAddedPercent * Dt), (AirMassAddedPercent * Dt));

			//============================= CLT

			if (TimeSec < 95)
				CLT = (int)float.Lerp(CLT, 95, (TimeSec + (MAP / 100.0f) * 10) / 95.0f);
			else
				CLT = 95;

			//============================ Target Lambda

			TargetLambda = MAPToTgtLambda(MAP);

			//=================== Current lambda

			for (int i = 1; i < LambdaQueue.Length; i++) {
				LambdaQueue[i - 1] = LambdaQueue[i];
			}
			LambdaQueue[LambdaQueue.Length - 1] = TargetLambda;

			float Speed = Math.Clamp((AirFlow - 30) / 50.0f, 0, 1);
			float RandomFactor = (Rnd.NextSingle() * 0.05f) - 0.025f;
			Lambda = LambdaQueue[(int)((LambdaQueue.Length - 1) * Speed)] + RandomFactor;



			//========================== Airflow

			float AirDensity = CalcDensityKgM3(MAP, IAT); // Kg/m3
			float DisplacementPerCyl = DisplacementL / NumOfCylinders; // L

			IntakeEfficiency = float.Lerp(0.70f, 0.95f, RPM / 6500.0f);

			float CylinderAirMass = IntakeEfficiency * CalcAirMassG(AirDensity, DisplacementPerCyl); // g/cyl
			float CalculatedAirMass = (CylinderAirMass * IntakeCyclesPerSecond); // g/s
			AirFlow = CalculatedAirMass;

			// Calculate VE

			float IdealAirDensity = KPaToPascal(100) / (SpecificGasConstant * ToKelvin(21)); // Kg/m3
			float IdealCylinderAirMass = IdealAirDensity * LToM3(DisplacementPerCyl) * 1000.0f; // g/s
			VE = (CylinderAirMass / IdealCylinderAirMass) * 100;

			//============================= Fueling

			float GasLambda = 14.7f;
			float CalculatedFuelMass = CalculatedAirMass / (TargetLambda * GasLambda); // g/s
			float CylinderFuelMass = CylinderAirMass / (TargetLambda * GasLambda); // g/cyl

			//============================== Injectors

			float InjSize = 6.52f; // Grams
			float InjOpenTime = 0.84f;


			FuelMass = CalculatedFuelMass;
			float InjPWTheoretical = (CalculatedFuelMass / InjSize);

			InjPW = InjPWTheoretical + InjOpenTime;
			InjPWDuty = ((InjPW / 1000.0f) * RoundsPerSecond * (1.0f / PowerStrokesPerRPM)) * 100;

			// Consume air
			OldAirMassInManifold = AirMassInManifold;
			AirMassInManifold = AirMassInManifold - CalculatedAirMass * Dt;
			if (AirMassInManifold < 0)
				AirMassInManifold = 0;

			float ConsumedAirMass = OldAirMassInManifold - AirMassInManifold;


			float MaxBurnTemp = float.Lerp(750, 1250, Math.Clamp((TargetLambda - 0.75f) / 0.25f, 0, 1));
			float BurnTempC = Utils.Weighted(200.0f, MaxBurnTemp, 1 - ConsumedAirMass, ConsumedAirMass);

			//============================ Exhaust 

			float ExAirDensity = CalcDensityKgM3(ExhaustManifoldPressureKPa, ExhaustManifoldTempC); // Kg/m3
			float ExAirMass = CalcAirMassG(ExAirDensity, ExhaustManifoldDisplacementL); // g

			float AddedMassPerc = ExMassInExhaust == 0 ? 1 : ExAirMass / ExMassInExhaust;
			if (AddedMassPerc > 1)
				AddedMassPerc = 1;
			if (AddedMassPerc < 0)
				AddedMassPerc = 0;

			ExMassInExhaust += ConsumedAirMass;
			float TempTransferFactor = Dt;
			ExhaustManifoldTempC = Utils.Weighted(ExhaustManifoldTempC, BurnTempC, 1 - (AddedMassPerc * TempTransferFactor), (AddedMassPerc * TempTransferFactor));

			//============================ Turbo 
			if (ExhaustManifoldPressureKPa > 300)
				WastegateDC = 0;

			WastegateDC = Math.Clamp(WastegateDC, 5, 95);

			float Wastegate = CalcWastegateFlow(WastegateDC / 100.0f);
			//float TurbineSwallowGrams = 70.0f;
			float TurboSwallowFactor = 0.1f;

			float OldExMassInExhaust = ExMassInExhaust;
			ExMassInExhaust = Utils.Weighted(ExMassInExhaust, ExMassInExhaust * TurboSwallowFactor, 1 - Dt, Dt) * Wastegate;
			if (ExMassInExhaust < 0)
				ExMassInExhaust = 0;

			float MassTroughTurbine = (OldExMassInExhaust - ExMassInExhaust) * (Wastegate);

			ExhaustManifoldPressureKPa = CalcPressureKPa(CalcAirDensity(ExMassInExhaust, ExhaustManifoldDisplacementL), ExhaustManifoldTempC);
			if (ExhaustManifoldPressureKPa < 0)
				ExhaustManifoldPressureKPa = 0;
			if (ExhaustManifoldPressureKPa > 1000)
				ExhaustManifoldPressureKPa = 1000;

			// Wastegate control
			//WastegateDC = float.Lerp(99, 1, Math.Clamp((ExhaustManifoldPressureKPa / (250 + 100)) - 0.5f, 0, 1));
			//WastegateDC = MAP > 180 ? 5 : 95;
			//WastegateDC = 5;

			TurboShaftSpeed = CalcTurboShaftSpeed(1.7f, MassTroughTurbine);
			//Console.WriteLine("TurboShaftSpeed = {0}", (int)TurboShaftSpeed);

			/*if (ExhaustManifoldPressureKPa > 300)
				WastegateDC = 90;

			if (ExhaustManifoldPressureKPa < 300)
				WastegateDC = 95;*/
		}

		static float ToCelsius(float Kelvin) {
			return Kelvin - 273.15f;
		}

		static float ToKelvin(float Celsius) {
			return Celsius + 273.15f;
		}

		static float ToBar(float Pascal) {
			return Pascal / 100000;
		}

		static float BarToPascal(float Bar) {
			return Bar * 100000.0f;
		}

		static float KPaToPascal(float KPa) {
			return KPa * 1000.0f;
		}

		static float LToM3(float L) {
			return L / 1000.0f;
		}

		static float ToHP(float kW) {
			return kW * 1.34102f;
		}
	}
}
