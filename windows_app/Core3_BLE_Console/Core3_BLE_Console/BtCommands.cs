using System;
using System.Collections.Generic;
using System.Diagnostics.Metrics;
using System.Linq;
using System.Runtime.InteropServices;
using System.Text;
using System.Threading.Tasks;

namespace Core3_BLE_Console {
	unsafe class BtCommands {
		byte Counter = 0;
		byte[] ReadMemoryArray;

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

		public IEnumerable<BtData> Cmd_CalRead(uint Offset, uint Size) {
			ReadMemoryArray = new byte[Size];

			for (uint x = 0; x < Size; x += 32) {
				uint CalcSize = 32;

				if (x < Size && (x + 32) > Size)
					CalcSize = Size - x;

				yield return CreateCommand(IDType.CAL_READ, Offset + x, CalcSize);
			}
		}

		public void Ret_CalResp(uint Offset, uint Size, byte[] DataArr) {
			Array.Copy(DataArr, 0, ReadMemoryArray, Offset, Size);
		}
	}
}
