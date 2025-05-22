using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;

namespace Core3_BLE_Console {
	record struct UndoData(int Offset, byte Value, byte OrigValue);

	class UndoBuffer {
		Stack<UndoData[]> UndoStack;

		public UndoBuffer() {
			UndoStack = new Stack<UndoData[]>();
		}

		public void Push(UndoData[] Dat) {
			UndoStack.Push(Dat);
		}

		public UndoData[] Pop() {
			if (UndoStack.Count == 0)
				return null;

			return UndoStack.Pop();
		}

		public void Clear() {
			UndoStack.Clear();
		}
	}
}
