using Plugin.BLE;
using Plugin.BLE.Abstractions;
using Plugin.BLE.Abstractions.Contracts;
using Plugin.BLE.Extensions;

using System.Text;

using Windows.Devices.Bluetooth;
using Windows.Devices.Bluetooth.GenericAttributeProfile;

namespace Core3_BLE_Console {
	internal class Program {
		static IBluetoothLE BLE;
		static IAdapter Adapter;

		static IDevice Core3Device;

		static void Main(string[] args) {
			Console.WriteLine("Starting");

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

				byte[] WriteBytes = Encoding.UTF8.GetBytes("Hello BLE C# World!0");
				WriteBytes[WriteBytes.Length - 1] = 0;

				Console.WriteLine("Write bytes len: {0}", WriteBytes.Length);
				int res = Characteristics[0].WriteAsync(WriteBytes).GetAwaiter().GetResult();
				Console.WriteLine("Write result: {0}", res);


				Thread.Sleep(1000);

				WriteBytes = Encoding.UTF8.GetBytes("Hello BLE C# World 2!0");
				WriteBytes[WriteBytes.Length - 1] = 0;

				Console.WriteLine("Write bytes len: {0}", WriteBytes.Length);
				res = Characteristics[0].WriteAsync(WriteBytes).GetAwaiter().GetResult();
				Console.WriteLine("Write result: {0}", res);


				while (true) {
					Thread.Sleep(1000);
				}
			}

			Console.WriteLine("Done!");
			Console.ReadLine();

		}

		private static void Program_ValueUpdated(object sender, Plugin.BLE.Abstractions.EventArgs.CharacteristicUpdatedEventArgs e) {
			byte[] Val = e.Characteristic.Value;
			Console.WriteLine("Data received! Len {0} = '{1}'", Val.Length, Encoding.ASCII.GetString(Val));
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
