using System;
using System.Collections.Generic;
using System.Diagnostics.Metrics;
using System.Linq;
using System.Runtime.InteropServices;
using System.Text;
using System.Threading.Tasks;

namespace Core3_BLE_Console {
	delegate void OnCalReadCompletedFunc(byte[] Memory);

	unsafe class BtCommands {
		byte Counter = 0;

		List<int> ReadMemoryCount = new List<int>();
		List<int> ReadMemoryReceived = new List<int>();

		byte[] ReadMemoryArray;
		OnCalReadCompletedFunc OnCalReadCompleted;
		Action OnCalWriteCompleted;
		Action OnEraseCompleted;

		BtData CreateCommand(IDType CmdID, uint Data1, uint Data2, uint Data3) {
			if (Counter >= 255)
				Counter = 0;

			BtData Dat = new BtData();
			Dat.ID = CmdID;
			Dat.Counter = Counter++;
			Dat.Data1 = Data1;
			Dat.Data2 = Data2;
			Dat.Data3 = Data3;

			return Dat;
		}

		public BtData[] Cmd_CalRead(uint Offset, uint Size, OnCalReadCompletedFunc OnCalReadCompleted) {
			ReadMemoryArray = new byte[Size];
			this.OnCalReadCompleted = OnCalReadCompleted;

			uint ReadSize = 16;

			List<BtData> Cmds = new List<BtData>();
			ReadMemoryCount.Clear();
			ReadMemoryReceived.Clear();

			for (uint x = 0; x < Size; x += ReadSize) {
				uint CalcSize = ReadSize;

				if (x < Size && (x + ReadSize) > Size)
					CalcSize = Size - x;

				BtData Cmd = CreateCommand(IDType.CAL_READ, Offset + x, CalcSize, Offset);
				ReadMemoryCount.Add(Cmd.Counter);
				Cmds.Add(Cmd);
			}

			return Cmds.ToArray();
		}

		public bool Ret_CalReadResp(int Counter, uint Offset, uint Size, uint Offset2, byte[] DataArr) {
			int Size2 = (int)Size;

			/*if (Offset + Size >= 940)
				Size2 = 940 - (int) Offset;

			if (Size2 < 0)
				return false;*/

			Offset = Offset - Offset2;

			if (Offset < ReadMemoryArray.Length) {
				Array.Copy(DataArr, 0, ReadMemoryArray, Offset, Size2);
			}

			ReadMemoryReceived.Add(Counter);

			foreach (var Count in ReadMemoryCount) {
				if (!ReadMemoryReceived.Contains(Count)) {
					return false;
				}
			}

			if (OnCalReadCompleted != null)
				OnCalReadCompleted(ReadMemoryArray);

			return true;
		}

		public BtData[] Cmd_CalWrite(uint Offset, uint Size, byte[] Data, Action OnCalWriteCompleted = null) {
			this.OnCalWriteCompleted = OnCalWriteCompleted;

			uint WriteSize = 16;

			List<BtData> Cmds = new List<BtData>();
			ReadMemoryCount.Clear();
			ReadMemoryReceived.Clear();

			for (uint x = 0; x < Size; x += WriteSize) {
				uint CalcSize = WriteSize;

				if (x < Size && (x + WriteSize) > Size)
					CalcSize = Size - x;

				BtData Cmd = CreateCommand(IDType.CAL_WRITE, Offset + x, CalcSize, Offset);

				for (int i = 0; i < CalcSize; i++) {
					Cmd.Data[i] = Data[x + i];
				}

				ReadMemoryCount.Add(Cmd.Counter);
				Cmds.Add(Cmd);
			}

			return Cmds.ToArray();
		}

		public bool Ret_CalWriteResp(int Counter) {
			ReadMemoryReceived.Add(Counter);

			foreach (var Count in ReadMemoryCount) {
				if (!ReadMemoryReceived.Contains(Count)) {
					return false;
				}
			}

			if (OnCalWriteCompleted != null)
				OnCalWriteCompleted();

			return true;
		}

		public BtData[] Cmd_CalErase(uint Offset, uint Size, Action OnEraseCompleted = null) {
			this.OnEraseCompleted = OnEraseCompleted;

			ReadMemoryCount.Clear();
			ReadMemoryReceived.Clear();

			List<BtData> Cmds = new List<BtData>();
			Cmds.Add(CreateCommand(IDType.CAL_ERASE, Offset, Size, 0));
			ReadMemoryCount.Add(Cmds[0].Counter);
			return Cmds.ToArray();
		}

		public bool Cmd_CalEraseResp(int Counter) {
			ReadMemoryReceived.Add(Counter);

			foreach (var Count in ReadMemoryCount) {
				if (!ReadMemoryReceived.Contains(Count)) {
					return false;
				}
			}

			if (OnEraseCompleted != null)
				OnEraseCompleted();

			return true;
		}

		public BtData[] Cmd_VarWatch(uint Var) {
			List<BtData> Cmds = new List<BtData>();
			Cmds.Add(CreateCommand(IDType.VAR_WATCH, Var, 1, 0));
			return Cmds.ToArray();
		}

		public bool Cmd_VarWatchResp(uint Var, uint Val) {
			//Console.WriteLine("{0} = {1}", Var, Val);
			return true;
		}

		public BtData[] Cmd_Reboot() {
			List<BtData> Cmds = new List<BtData>();
			Cmds.Add(CreateCommand(IDType.VAR_RBOOT, 0, 0, 0));
			return Cmds.ToArray();
		}

		public bool Cmd_RebootResp(uint Var, uint Val) {
			//Console.WriteLine("{0} = {1}", Var, Val);
			return true;
		}

		public BtData[] Cmd_Hello() {
			List<BtData> Cmds = new List<BtData>();
			Cmds.Add(CreateCommand(IDType.HELLO, 3, 2, 1));
			return Cmds.ToArray();
		}

		public bool Cmd_HelloResp(uint Var1, uint Var2, uint Var3) {
			Console.WriteLine("[Bluetooth] Hello!");
			return true;
		}
	}
}
