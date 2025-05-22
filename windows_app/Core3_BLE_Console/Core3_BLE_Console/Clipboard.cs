using System;
using System.Collections.Generic;
using System.ComponentModel;
using System.Linq;
using System.Numerics;
using System.Runtime.InteropServices;
using System.Text;
using System.Threading.Tasks;

using Raylib_cs;

using TextCopy;


namespace Core3_BLE_Console {
	static class WindowsClipboard {
		public static void SetText(string text) {
			ClipboardService.SetText(text);
		}

		public static string GetText() {
			return ClipboardService.GetText() ?? "";
		}
	}
}
