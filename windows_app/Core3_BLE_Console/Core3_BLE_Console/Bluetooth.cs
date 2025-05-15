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

		public static void DoBluetooth() {
			BtThread = new Thread(BluetoothThread);
			BtThread.IsBackground = true;
			BtThread.Start();
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
					Thread.Sleep(1);

					DataQueue.Update((DataBytes) => {
						BluetoothSend(Characteristics[0], DataBytes);
					});
				}
			}
		}

		static void CommandHandlerThread() {
			BtData[] CmdArr = DataQueue.Commands.Cmd_CalRead(0x0, 600).ToArray();

			foreach (BtData Cmd in CmdArr) {
				while (!DataQueue.TryEnqueueSend(Cmd))
					Thread.Sleep(10);
			}
		}

		static int BluetoothSend(ICharacteristic Char, byte[] DataBytes) {
			//Console.WriteLine("BtSending ID {0}, Counter {1}", Cmd.ID, Cmd.Counter);

			return Char.WriteAsync(DataBytes).GetAwaiter().GetResult();
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
				Core3Device = e.Device;
			}
		}
	}
}
