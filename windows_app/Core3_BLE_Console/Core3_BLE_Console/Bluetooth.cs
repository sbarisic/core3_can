using Plugin.BLE;
using Plugin.BLE.Abstractions;
using Plugin.BLE.Abstractions.Contracts;
using Plugin.BLE.Extensions;


using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;

using Windows.Devices.Bluetooth;
using Windows.Devices.Bluetooth.GenericAttributeProfile;

using System.Runtime.InteropServices;
using static System.Runtime.InteropServices.JavaScript.JSType;

namespace Core3_BLE_Console {
	enum IDType : byte {
		CAL_READ = 0x1, // uint32_T Data1 - Offset, uint32_t Data2 - Length
		CAL_RESP = 0x2, // 32 bytes of data
	}

	[StructLayout(LayoutKind.Explicit, Pack = 1)]
	unsafe struct BtData {
		[FieldOffset(0)]
		public IDType ID;

		[FieldOffset(1)]
		public byte Counter;

		[FieldOffset(2)]
		public fixed byte Data[32];

		[FieldOffset(2)]
		public uint Data1;

		[FieldOffset(2 + sizeof(uint))]
		public uint Data2;
	}

	unsafe static class Bluetooth {
		static IBluetoothLE BLE;
		static IAdapter Adapter;
		static IDevice Core3Device;

		static Thread BtThread, CmdHandlerThread;

		static byte Counter = 0;
		static BtData?[] BtCommandArray = new BtData?[16];

		static BtData?[] BtReturnArray = new BtData?[16];

		static object Lck = new object();

		public static void DoBluetooth() {
			BtThread = new Thread(BluetoothThread);
			BtThread.IsBackground = true;
			BtThread.Start();
		}

		static void BluetoothThread() {
			BLE = CrossBluetoothLE.Current;
			Adapter = CrossBluetoothLE.Current.Adapter;

			Adapter.DeviceDiscovered += Adapter_DeviceDiscovered;
			Adapter.ScanTimeout = 2000;
			Adapter.ScanMode = ScanMode.LowLatency;
			Adapter.StartScanningForDevicesAsync().GetAwaiter().GetResult();


			if (Core3Device != null) {
				Console.WriteLine("Connecting to Core 3 Console");

				BluetoothLEDevice BLE_Device = BluetoothLEDevice.FromBluetoothAddressAsync(Core3Device.Id.ToBleAddress()).GetAwaiter().GetResult();
				BluetoothDeviceId BLE_DevID = BluetoothDeviceId.FromId(BLE_Device.DeviceId);

				GattSession BLE_Session = GattSession.FromDeviceIdAsync(BLE_DevID).GetAwaiter().GetResult();
				BLE_Session.MaintainConnection = true;


				IService[] Services = Core3Device.GetServicesAsync().GetAwaiter().GetResult().ToArray();
				ICharacteristic[] Characteristics = Services[2].GetCharacteristicsAsync().GetAwaiter().GetResult().ToArray();

				Characteristics[1].ValueUpdated += Program_ValueUpdated;
				Characteristics[1].StartUpdatesAsync().GetAwaiter().GetResult();

				Thread.Sleep(1000);

				CmdHandlerThread = new Thread(CommandHandlerThread);
				CmdHandlerThread.IsBackground = true;
				CmdHandlerThread.Start();



				while (true) {
					Thread.Sleep(500);

					lock (Lck) {
						if (TryDequeueSend(out BtData BtCmd)) {
							Console.WriteLine(">> TryDequeueSend Counter {0}", BtCmd.Counter);

							if (TryAddReturnList(BtCmd)) {
								BluetoothSend(Characteristics[0], BtCmd);
							} else {
								TryEnqueueSend(BtCmd);
							}
						}
					}
				}
			}
		}

		static void CommandHandlerThread() {
			BtData[] CmdArr = CreateReadMemory(0x0, 0x256).ToArray();

			foreach (BtData Cmd in CmdArr) {
				while (!TryEnqueueSend(Cmd))
					Thread.Sleep(10);
			}
		}

		static byte[] ReadMemoryArray;

		static IEnumerable<BtData> CreateReadMemory(uint Offset, uint Size) {
			ReadMemoryArray = new byte[Size];

			for (uint x = 0; x < Size; x += 32) {
				uint CalcSize = 32;

				if (x < Size && (x + 32) > Size)
					CalcSize = Size - x;

				yield return CreateCommand(IDType.CAL_READ, Offset + x, CalcSize);
			}
		}

		static bool ProcessData(BtData Orig, BtData Return) {
			if (Orig.ID == IDType.CAL_READ && Return.ID == IDType.CAL_RESP) {
				Console.WriteLine("Got a READ response");

				byte[] DataArr = new byte[32];
				for (int i = 0; i < DataArr.Length; i++) {
					DataArr[i] = Return.Data[i];
				}

				Array.Copy(DataArr, 0, ReadMemoryArray, Orig.Data1, Orig.Data2);
			}

			return false;
		}

		static bool TryAddReturnList(BtData Cmd) {
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

		static bool TryFindAnyReturnList(out BtData Cmd) {
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

		static bool IsIDMatchingPair(BtData Orig, BtData Return) {
			if (Orig.ID == (Return.ID - 1))
				return true;

			return false;
		}

		static bool TryFindReturnList(BtData Return, out BtData Orig) {
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

		static bool TryEnqueueSend(BtData Cmd) {
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

		static bool TryDequeueSend(out BtData Cmd) {
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

		static int BluetoothSend(ICharacteristic Char, BtData Cmd) {
			//Console.WriteLine("BtSending ID {0}, Counter {1}", Cmd.ID, Cmd.Counter);

			return Char.WriteAsync(SerializeCommand(Cmd)).GetAwaiter().GetResult();
		}

		static bool TryParseCommand(byte[] Bytes, out BtData Dat) {
			Dat = new BtData();

			if (Bytes.Length < sizeof(BtData))
				return false;

			fixed (byte* BytesPtr = Bytes) {
				Dat = Marshal.PtrToStructure<BtData>((IntPtr)BytesPtr);
			}

			return true;
		}

		static byte[] SerializeCommand(BtData Cmd) {
			byte[] Mem = new byte[sizeof(BtData)];

			fixed (byte* MemPtr = Mem) {
				Marshal.StructureToPtr(Cmd, (IntPtr)MemPtr, false);
			}

			return Mem;
		}

		static BtData CreateCommand(IDType CmdID, uint Data1, uint Data2) {
			BtData Dat = new BtData();
			Dat.ID = CmdID;
			Dat.Counter = Counter++;
			Dat.Data1 = Data1;
			Dat.Data2 = Data2;

			return Dat;
		}

		private static void Program_ValueUpdated(object sender, Plugin.BLE.Abstractions.EventArgs.CharacteristicUpdatedEventArgs e) {
			byte[] Val = e.Characteristic.Value;
			//Console.WriteLine("Data received! Len {0}", Val.Length);

			if (TryParseCommand(Val, out BtData Dat)) {
				//Console.WriteLine("Parse: ID {0}, Counter {1}", Dat.ID, Dat.Counter);

				if (TryFindReturnList(Dat, out BtData Orig)) {
					ProcessData(Orig, Dat);
				}
			}
		}

		private static void Adapter_DeviceDiscovered(object sender, Plugin.BLE.Abstractions.EventArgs.DeviceEventArgs e) {
			Console.WriteLine(">> {0} (RSSI {1})", e.Device.Name, e.Device.Rssi);

			if (e.Device.Name == "ESP_SPP_SERVER") {
				Console.WriteLine("Found Core3 Console");
				Core3Device = e.Device;
			}
		}
	}
}
