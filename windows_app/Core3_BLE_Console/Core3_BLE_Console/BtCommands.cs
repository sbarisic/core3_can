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

		BtData CreateCommand(IDType CmdID, uint Data1, uint Data2) {
			if (Counter >= 255)
				Counter = 0;

			BtData Dat = new BtData();
			Dat.ID = CmdID;
			Dat.Counter = Counter++;
			Dat.Data1 = Data1;
			Dat.Data2 = Data2;

			return Dat;
		}

		public BtData[] Cmd_CalRead(uint Offset, uint Size, OnCalReadCompletedFunc OnCalReadCompleted) {
			ReadMemoryArray = new byte[Size];
			this.OnCalReadCompleted = OnCalReadCompleted;

			List<BtData> Cmds = new List<BtData>();
			ReadMemoryCount.Clear();
			ReadMemoryReceived.Clear();

			for (uint x = 0; x < Size; x += 32) {
				uint CalcSize = 32;

				if (x < Size && (x + 32) > Size)
					CalcSize = Size - x;

				BtData Cmd = CreateCommand(IDType.CAL_READ, Offset + x, CalcSize);
				ReadMemoryCount.Add(Cmd.Counter);
				Cmds.Add(Cmd);
			}

			return Cmds.ToArray();
		}

		public bool Ret_CalReadResp(int Counter, uint Offset, uint Size, byte[] DataArr) {
			Array.Copy(DataArr, 0, ReadMemoryArray, Offset, Size);
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

			List<BtData> Cmds = new List<BtData>();
			ReadMemoryCount.Clear();
			ReadMemoryReceived.Clear();

			for (uint x = 0; x < Size; x += 32) {
				uint CalcSize = 32;

				if (x < Size && (x + 32) > Size)
					CalcSize = Size - x;

				BtData Cmd = CreateCommand(IDType.CAL_WRITE, Offset + x, CalcSize);

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
			Cmds.Add(CreateCommand(IDType.CAL_ERASE, Offset, Size));
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
	}
}
