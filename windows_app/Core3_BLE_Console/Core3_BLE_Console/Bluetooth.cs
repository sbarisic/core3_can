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
	unsafe static class Bluetooth {
		static IBluetoothLE BLE;
		static IAdapter Adapter;
		static IDevice Core3Device;

		static Thread BtThread, CmdHandlerThread;
		static BtDataQueue DataQueue;

		static BluetoothLEDevice BLE_Device = null;
		static ICharacteristic BLE_Char = null;
		static bool BLE_IsConnected;

		public static void DoBluetooth() {
			DataQueue = null;

			BtThread = new Thread(BluetoothThread);
			BtThread.IsBackground = true;
			BtThread.Start();

			while (DataQueue == null)
				Thread.Sleep(1);
		}

		static void BluetoothThread() {
			DataQueue = new BtDataQueue();

			BLE = CrossBluetoothLE.Current;
			Adapter = CrossBluetoothLE.Current.Adapter;

			Adapter.DeviceDiscovered += Adapter_DeviceDiscovered;
			Adapter.ScanTimeout = 2000;
			Adapter.ScanMode = ScanMode.LowLatency;
			Adapter.StartScanningForDevicesAsync().GetAwaiter().GetResult();


			if (Core3Device != null) {
				Console.WriteLine("Connecting to Core 3 Console");

				ReInit();

				//CmdHandlerThread = new Thread(CommandHandlerThread);
				//CmdHandlerThread.IsBackground = true;
				//CmdHandlerThread.Start();

				while (true) {
					Thread.Sleep(0);

					DataQueue.Update((DataBytes) => {
						BluetoothSend(DataBytes);
					});
				}
			}
		}

		static void ReInit() {
			BLE_Device = BluetoothLEDevice.FromBluetoothAddressAsync(Core3Device.Id.ToBleAddress()).GetAwaiter().GetResult();
			BluetoothDeviceId BLE_DevID = BluetoothDeviceId.FromId(BLE_Device.DeviceId);
			Thread.Sleep(10);

			GattSession BLE_Session = GattSession.FromDeviceIdAsync(BLE_DevID).GetAwaiter().GetResult();
			BLE_Session.MaintainConnection = true;
			BLE_Device.ConnectionStatusChanged += BLE_Device_ConnectionStatusChanged;
			Thread.Sleep(200);


			IService[] Services = Core3Device.GetServicesAsync().GetAwaiter().GetResult().ToArray();
			Thread.Sleep(200);

			ICharacteristic[] Characteristics = null;
			for (int i = 0; i < 5; i++) {
				try {
					Characteristics = Services[2].GetCharacteristicsAsync().GetAwaiter().GetResult().ToArray();
					break;
				} catch (Exception) {
				}
				Thread.Sleep(200);
			}

			Thread.Sleep(200);

			Characteristics[1].ValueUpdated += Program_ValueUpdated;
			Characteristics[1].StartUpdatesAsync().GetAwaiter().GetResult();

			Thread.Sleep(10);
			BLE_Char = Characteristics[0];

			while (BLE_Device.ConnectionStatus != BluetoothConnectionStatus.Connected)
				Thread.Sleep(10);
		}

		private static void BLE_Device_ConnectionStatusChanged(BluetoothLEDevice sender, object args) {
			Console.WriteLine("ConStatus: {0}", sender.ConnectionStatus);
			BLE_IsConnected = sender.ConnectionStatus == BluetoothConnectionStatus.Connected;

			if (!BLE_IsConnected) {
				Thread.Sleep(1000);
				Adapter.StartScanningForDevicesAsync().GetAwaiter().GetResult();
			}
		}

		public static bool IsConnected() {
			if (BLE_Device == null || Core3Device == null)
				return false;

			return BLE_IsConnected;
		}

		public static BtDataQueue GetDataQueue() {
			return DataQueue;
		}

		static void CommandHandlerThread() {
			BtData[] CmdArr = DataQueue.Commands.Cmd_CalRead(0x0, 600, (Mem) => {
				Console.WriteLine("Mem: {0}", Mem.Length);
			}).ToArray();

			foreach (BtData Cmd in CmdArr) {
				while (!DataQueue.TryEnqueueSend(Cmd))
					Thread.Sleep(10);
			}
		}

		static int BluetoothSend(byte[] DataBytes) {
			//Console.WriteLine("BtSending ID {0}, Counter {1}", Cmd.ID, Cmd.Counter);

			return BLE_Char.WriteAsync(DataBytes).GetAwaiter().GetResult();
		}

		private static void Program_ValueUpdated(object sender, Plugin.BLE.Abstractions.EventArgs.CharacteristicUpdatedEventArgs e) {
			byte[] Val = e.Characteristic.Value;
			//Console.WriteLine("Data received! Len {0}", Val.Length);

			DataQueue.OnDataReceived(Val);
		}

		private static void Adapter_DeviceDiscovered(object sender, Plugin.BLE.Abstractions.EventArgs.DeviceEventArgs e) {
			Console.WriteLine(">> {0} (RSSI {1})", e.Device.Name, e.Device.Rssi);

			if (e.Device.Name == "ESP_SPP_SERVER") {
				Console.WriteLine("Found Core3 Console");

				if (Core3Device != null) {
					Core3Device.Dispose();
					Core3Device = null;
				}

				Core3Device = e.Device;
				//ReInit();
			}
		}
	}
}
