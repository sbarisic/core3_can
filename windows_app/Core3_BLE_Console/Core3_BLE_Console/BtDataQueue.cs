using System;
using System.Collections.Generic;
using System.Linq;
using System.Runtime.InteropServices;
using System.Text;
using System.Threading.Tasks;

namespace Core3_BLE_Console {
	enum IDType : byte {
		CAL_READ = 0x1, // uint32_T Data1 - Offset, uint32_t Data2 - Length
		CAL_READ_RESP = 0x2, // 32 bytes of data

		CAL_WRITE,
		CAL_WRITE_RESP,

		CAL_ERASE,
		CAL_ERASE_RESP,

		RAM_READ,
		RAM_READ_RESP,
	}

	[StructLayout(LayoutKind.Sequential, Pack = 1)]
	unsafe struct BtData {
		public IDType ID;
		public byte Counter;

		public uint Data1;
		public uint Data2;

		public fixed byte Data[32];

	}

	delegate void BtSendFunc(byte[] SendBytes);

	unsafe class BtDataQueue {
		byte Counter = 0;
		BtData?[] BtCommandArray = new BtData?[16];
		BtData?[] BtReturnArray = new BtData?[16];

		object Lck = new object();

		public BtCommands Commands = new BtCommands();

		public BtDataQueue() {

		}

		public void Update(BtSendFunc BtSend) {
			lock (Lck) {
				if (TryDequeueSend(out BtData BtCmd)) {
					Console.WriteLine(">> TryDequeueSend Counter {0}", BtCmd.Counter);

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
				}
			}
		}

		bool ProcessData(BtData Orig, BtData Return) {
			if (Orig.ID == IDType.CAL_READ && Return.ID == IDType.CAL_READ_RESP) {
				Console.WriteLine("Got a READ response (Cnt {0}, Len {1})", Return.Counter, Orig.Data2);

				byte[] DataArr = new byte[32];
				for (int i = 0; i < DataArr.Length; i++) {
					DataArr[i] = Return.Data[i];
				}

				Commands.Ret_CalReadResp(Orig.Counter, Orig.Data1, Orig.Data2, DataArr);
			}

			return false;
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
