import asyncio
from bleak import BleakClient, BleakScanner
import time

# Optional: OpenCV to display/process image
import cv2
import numpy as np

SERVICE_UUID = "4fafc201-1fb5-459e-8fcc-c5c9c331914b"
CHARACTERISTIC_UUID = "beb5483e-36e1-4688-b7f5-ea07361b26a8"

image_buffer = bytearray()
frame_complete = False

def notification_handler(_, data: bytearray):
    global image_buffer
    image_buffer.extend(data)
    print(f"Received chunk: {len(data)} bytes, total: {len(image_buffer)}")

async def receive_image(address: str):
    global image_buffer

    print(f"Connecting to {address}...")
    async with BleakClient(address) as client:
        print("Connected. Starting notifications...")
        await client.start_notify(CHARACTERISTIC_UUID, notification_handler)

        # Wait and accumulate the image data
        print("Receiving image. Press Ctrl+C to stop early.")
        image_buffer.clear()
        await asyncio.sleep(5)  # Time window to receive one image

        await client.stop_notify(CHARACTERISTIC_UUID)
        print("Stopped notifications.")

        if len(image_buffer) < 100:
            print("⚠️ Received too little data. Something went wrong.")
            return

        # Save to file
        with open("output.jpg", "wb") as f:
            f.write(image_buffer)
            print(f"Image saved as output.jpg ({len(image_buffer)} bytes)")

        # Optional: Display with OpenCV
        img_np = np.frombuffer(image_buffer, dtype=np.uint8)
        img = cv2.imdecode(img_np, cv2.IMREAD_COLOR)
        if img is not None:
            cv2.imshow("ESP32-CAM Image", img)
            cv2.waitKey(0)
            cv2.destroyAllWindows()
        else:
            print("Failed to decode image with OpenCV.")

async def main():
    print("🔍 Scanning for BLE devices...")
    devices = await BleakScanner.discover()
    for i, d in enumerate(devices):
        print(f"[{i}] {d.name} - {d.address}")

    idx = int(input("Enter the number of your ESP32 device: "))
    esp32_address = devices[idx].address

    await receive_image(esp32_address)

if __name__ == "__main__":
    try:
        asyncio.run(main())
    except KeyboardInterrupt:
        print("\nStopped by user")
