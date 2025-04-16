import asyncio
from bleak import BleakClient, BleakScanner

SERVICE_UUID = "4fafc201-1fb5-459e-8fcc-c5c9c331914b"
CHARACTERISTIC_UUID = "beb5483e-36e1-4688-b7f5-ea07361b26a8"

def handle_ble_message(_, data: bytearray):
    try:
        message = data.decode("utf-8", errors="ignore")
        print(f"[CAM OUTPUT]: {message.strip()}")
    except Exception as e:
        print(f"Decode error: {e}")

async def main():
    print("Scanning...")
    devices = await BleakScanner.discover()
    for i, d in enumerate(devices):
        print(f"[{i}] {d.name} - {d.address}")
    
    idx = int(input("Enter ESP32 device number: "))
    esp32_address = devices[idx].address

    async with BleakClient(esp32_address) as client:
        print("Connected.")
        await client.start_notify(CHARACTERISTIC_UUID, handle_ble_message)
        print("Listening to inference output. Ctrl+C to exit.")
        try:
            while True:
                await asyncio.sleep(1)
        except KeyboardInterrupt:
            await client.stop_notify(CHARACTERISTIC_UUID)
            print("Disconnected.")

asyncio.run(main())
