using System;
using System.Collections.Generic;
using System.Linq;
using System.Runtime.InteropServices;
using System.Text;
using System.Threading.Tasks;

namespace Core3_BLE_Console {
	enum IDType : byte {
		NULL,

		CAL_READ, // uint32_T Data1 - Offset, uint32_t Data2 - Length
		CAL_READ_RESP, // 32 bytes of data

		CAL_WRITE,
		CAL_WRITE_RESP,

		CAL_ERASE,
		CAL_ERASE_RESP,

		RAM_READ,
		RAM_READ_RESP,

		VAR_WATCH,
		VAR_WATCH_RESP,

		VAR_RBOOT,
		VAR_RBOOT_RESP,

		HELLO,
		HELLO_RESP
	}

	[StructLayout(LayoutKind.Sequential, Pack = 1)]
	unsafe struct BtData {
		public IDType ID;
		public byte Counter;

		public uint Data1;
		public uint Data2;
		public uint Data3;

		public fixed byte Data[32];
	}

	[StructLayout(LayoutKind.Sequential, Pack = 1)]
	unsafe struct Core3MapAxis {
		public int len;
		ushort[] axis;
	}

	[StructLayout(LayoutKind.Sequential, Pack = 1)]
	unsafe struct Core3Map {
		public Core3MapAxis X;
		public Core3MapAxis Y;

		public byte[] Memory;
	}

	delegate void BtSendFunc(byte[] SendBytes);

	enum VarUnit : byte {
		RAW = 0,
		VOLT
	}

	enum ECUVariable : int {
		VAR_ANALOG0 = 1,
		VAR_ANALOG1,
		VAR_ANALOG2,
		VAR_ANALOG3,
		VAR_RPM,
		VAR_MAP,
		VAR_LTFT,
		VAR_OCTANE_FACTOR,

		VAR_ERR_CLT,
		VAR_ERR_IAT,
		VAR_ERR_MAP,
		VAR_ERR_WBO,
		VAR_KNOCK
	}

	class BtWatcherVariable {
		public string Name;
		public uint ID;
		public uint Value;
		public float Time;
		public float ValueFloat;

		public BtWatcherVariable(string Name, uint ID, uint Value, float Time, float ValueFloat) {
			this.Name = Name;
			this.ID = ID;
			this.Value = Value;
			this.Time = Time;
			this.ValueFloat = ValueFloat;
		}

		public override string ToString() {
			if (!string.IsNullOrEmpty(Name)) {
				return string.Format("{0} = {1}", Name.Trim(), ValueFloat);
			}

			return ValueFloat.ToString();
		}
	}

	unsafe class BtDataQueue {
		byte Counter = 0;
		BtData?[] BtCommandArray = new BtData?[16];
		BtData?[] BtReturnArray = new BtData?[16];

		object Lck = new object();

		public BtCommands Commands = new BtCommands();

		public Dictionary<uint, BtWatcherVariable> Variables = new Dictionary<uint, BtWatcherVariable>();

		public BtDataQueue() {

		}

		public void Update(BtSendFunc BtSend) {
			lock (Lck) {
				if (TryDequeueSend(out BtData BtCmd)) {
					//Console.WriteLine(">> TryDequeueSend Counter {0}", BtCmd.Counter);

					if (TryAddReturnList(BtCmd)) {
						//BluetoothSend(Characteristics[0], BtCmd);
						BtSend(SerializeCommand(BtCmd));
					} else {
						TryEnqueueSend(BtCmd);
					}
				}
			}
		}

		public void OnDataReceived(byte[] Val) {
			if (TryParseCommand(Val, out BtData Dat)) {
				//Console.WriteLine("Parse: ID {0}, Counter {1}", Dat.ID, Dat.Counter);

				if (TryFindReturnList(Dat, out BtData Orig)) {
					ProcessData(Orig, Dat);
				} else
					ProcessDataReturn(Dat);
			}
		}

		bool ProcessData(BtData Orig, BtData Return) {
			if (Orig.ID == IDType.CAL_READ && Return.ID == IDType.CAL_READ_RESP) {
				Console.WriteLine("Got a READ response (Cnt {0}, Len {1})", Return.Counter, Orig.Data2);

				byte[] DataArr = new byte[32];
				for (int i = 0; i < DataArr.Length; i++) {
					DataArr[i] = Return.Data[i];
				}

				Commands.Ret_CalReadResp(Orig.Counter, Orig.Data1, Orig.Data2, Orig.Data3, DataArr);
			} else if (Orig.ID == IDType.HELLO && Return.ID == IDType.HELLO_RESP) {
				Commands.Cmd_HelloResp(Return.Data1, Return.Data2, Return.Data3);
			}

			return false;
		}

		byte[] NameBytes = new byte[8];

		bool ProcessDataReturn(BtData Return) {
			if (Return.ID == IDType.VAR_WATCH_RESP) {
				uint Var = Return.Data1;
				uint Val = Return.Data2;
				float Sec = ((float*)Return.Data)[0];
				float Valf = ((float*)Return.Data)[1];

				for (int i = 0; i < 8; i++) {
					NameBytes[i] = Return.Data[8 + i];
				}

				string Name = Encoding.ASCII.GetString(NameBytes);

				if (Variables.ContainsKey(Var)) {
					Variables[Var].Value = Val;
					Variables[Var].Time = Sec;
					Variables[Var].ValueFloat = Valf;
					Variables[Var].Name = Name;
				} else {
					Variables[Var] = new BtWatcherVariable(Name, Var, Val, Sec, Valf);
				}

				//Console.WriteLine("[{0}] {1} = {2}", MathF.Round(Sec, 4), Var, Val);
			} else if (Return.ID == IDType.VAR_RBOOT_RESP) {
				Console.WriteLine("ECU Rebooting");
			}

			return false;
		}

		public BtWatcherVariable GetVariable(string Name, ECUVariable EcuVar) {
			uint Var = (uint)EcuVar;

			if (Variables.ContainsKey(Var))
				return Variables[Var];

			Variables.Add(Var, new BtWatcherVariable(Name, Var, 0, 0, 0));
			return Variables[Var];
		}

		public bool TryParseCommand(byte[] Bytes, out BtData Dat) {
			Dat = new BtData();

			if (Bytes.Length < sizeof(BtData))
				return false;

			fixed (byte* BytesPtr = Bytes) {
				Dat = Marshal.PtrToStructure<BtData>((IntPtr)BytesPtr);
			}

			return true;
		}

		public byte[] SerializeCommand(BtData Cmd) {
			byte[] Mem = new byte[sizeof(BtData)];

			fixed (byte* MemPtr = Mem) {
				Marshal.StructureToPtr(Cmd, (IntPtr)MemPtr, false);
			}

			return Mem;
		}

		public bool TryAddReturnList(BtData Cmd) {
			lock (Lck) {
				for (int i = 0; i < BtReturnArray.Length; i++) {
					if (BtReturnArray[i] == null) {
						BtReturnArray[i] = Cmd;
						return true;
					}
				}
			}

			return false;
		}

		public bool TryFindAnyReturnList(out BtData Cmd) {
			lock (Lck) {
				for (int i = BtReturnArray.Length - (1); i >= 0; i--) {
					if (BtReturnArray[i] != null) {
						Cmd = BtReturnArray[i].Value;
						return true;
					}
				}
			}

			Cmd = new BtData();
			return false;
		}

		public bool IsIDMatchingPair(BtData Orig, BtData Return) {
			if (Orig.ID == (Return.ID - 1))
				return true;

			return false;
		}

		public bool TryFindReturnList(BtData Return, out BtData Orig) {
			lock (Lck) {
				for (int i = 0; i < BtReturnArray.Length; i++) {
					if (BtReturnArray[i] != null && IsIDMatchingPair(BtReturnArray[i].Value, Return) && BtReturnArray[i].Value.Counter == Return.Counter) {
						Orig = BtReturnArray[i].Value;
						BtReturnArray[i] = null;
						return true;
					}
				}
			}

			Orig = new BtData();
			return false;
		}

		public bool IsSending() {
			lock (Lck) {
				for (int i = 0; i < BtCommandArray.Length; i++) {
					if (BtCommandArray[i] != null) {
						return true;
					}
				}
			}

			return false;
		}

		public void FlushSendQueue() {
			while (IsSending()) {
				Thread.Sleep(1);
			}
		}

		public bool TryEnqueueSend(BtData Cmd) {
			lock (Lck) {
				for (int i = 0; i < BtCommandArray.Length; i++) {
					if (BtCommandArray[i] == null) {
						BtCommandArray[i] = Cmd;
						return true;
					}
				}
			}

			return false;
		}

		public bool TryDequeueSend(out BtData Cmd) {
			lock (Lck) {
				for (int i = 0; i < BtCommandArray.Length; i++) {
					if (BtCommandArray[i] != null) {
						Cmd = BtCommandArray[i].Value;
						BtCommandArray[i] = null;
						return true;
					}
				}
			}

			Cmd = new BtData();
			return false;
		}
	}
}
