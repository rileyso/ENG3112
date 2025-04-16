import asyncio
import threading
# from tkinter import Tk, Text, Scrollbar, END, RIGHT, LEFT, Y, BOTH, TOP, Frame
from bleak import BleakClient, BleakScanner
from tkinter import Tk, Text, Scrollbar, Button, Frame, END, RIGHT, LEFT, Y, BOTH, TOP
from tkinter import Listbox, StringVar, Label
from datetime import datetime
SERVICE_UUID = "4fafc201-1fb5-459e-8fcc-c5c9c331914b"
CHARACTERISTIC_UUID = "beb5483e-36e1-4688-b7f5-ea07361b26a8"

SODIUM_CONTENT = {
    "UpnGo": 250,
    "John West Tuna": 324,
    # Add more known items here
}

class BLEConsoleGUI:
    def __init__(self, root):
        self.root = root
        self.root.title("HEARTY APP GROUP10")

        # --- Top Control Buttons ---
        top_frame = Frame(root)
        top_frame.pack(side=TOP, fill="x", pady=5)

        reconnect_button = Button(top_frame, text="Reconnect", command=self.reconnect_ble)
        reconnect_button.pack(side=LEFT, padx=5)

        clear_button = Button(top_frame, text="Clear", command=self.clear_console)
        clear_button.pack(side=LEFT, padx=5)

        # --- Main Layout ---
        content_frame = Frame(root)
        content_frame.pack(side=TOP, fill=BOTH, expand=True)

        # --- BLE Console Output (Left Side) ---
        self.text_area = Text(content_frame, wrap="word", height=25, width=80)
        self.text_area.tag_config("green", foreground="green")
        self.scrollbar = Scrollbar(content_frame, command=self.text_area.yview)
        self.text_area.configure(yscrollcommand=self.scrollbar.set)
        self.text_area.pack(side=LEFT, fill=BOTH, expand=True)
        self.scrollbar.pack(side=LEFT, fill=Y)

        # --- Catalogue (Right Side) ---
        catalogue_frame = Frame(content_frame)
        catalogue_frame.pack(side=RIGHT, fill=Y, padx=5)

        self.catalogue_header = Label(
            catalogue_frame,
            text="Name".ljust(12) + "Qty".ljust(8) + "Time".ljust(12) + "Sodium",
            font=("Courier", 10, "bold"),
            anchor="w",
            justify=LEFT
        )
        self.catalogue_header.pack(anchor="w")

        self.catalogue_listbox = Listbox(catalogue_frame, width=40, font=("Courier", 10))
        self.catalogue_listbox.pack(fill=Y)

        self.catalogue = {}

        self.loop = asyncio.new_event_loop()
        self.ble_client = None
        threading.Thread(target=self.start_ble_loop, daemon=True).start()

    def reconnect_ble(self):
        self.append_text("Reconnecting BLE...")

        async def cleanup_and_restart():
            try:
                if self.ble_client and self.ble_client.is_connected:
                    await self.ble_client.stop_notify(CHARACTERISTIC_UUID)
                    await self.ble_client.disconnect()
                    self.append_text("Disconnected BLE client.")
            except Exception as e:
                self.append_text(f"Error during disconnect: {e}")
            
            # Restart BLE loop
            await self.run_ble_client()

        # Run the coroutine in the loop thread
        asyncio.run_coroutine_threadsafe(cleanup_and_restart(), self.loop)


    def clear_console(self):
        self.text_area.delete(1.0, END)

    def append_text(self, message):
        if "Detected" in message:
            self.text_area.insert(END, message + "\n", "green")
        else:
            self.text_area.insert(END, message + "\n")
        self.text_area.see(END)


    # Update catalogue helper
    def update_catalogue(self, name, confidence):
        now = datetime.now().strftime("%H:%M:%S")
        sodium = SODIUM_CONTENT.get(name, "?")

        if name in self.catalogue:
            self.catalogue[name]["count"] += 1
            self.catalogue[name]["last_seen"] = now
            self.catalogue[name]["confidence"] = confidence
        else:
            self.catalogue[name] = {
                "count": 1,
                "last_seen": now,
                "confidence": confidence,
                "sodium": sodium
            }

        # Refresh display
        self.catalogue_listbox.delete(0, END)

        for obj, data in self.catalogue.items():
            line = (
                obj.ljust(12) +
                f"x{data['count']}".ljust(8) +
                data['last_seen'].ljust(12) +
                f"{str(data['sodium'])}mg"
            )
            self.catalogue_listbox.insert(END, line)

    async def run_ble_client(self):
        self.append_text("Scanning for BLE devices...")
        devices = await BleakScanner.discover()
        esp32 = None
        for d in devices:
            if "ESP32" in (d.name or ""):
                esp32 = d
                break

        if not esp32:
            self.append_text("ESP32 device not found.")
            return

        self.append_text(f"Connecting to {esp32.name} - {esp32.address}")
        self.ble_client = BleakClient(esp32.address)
        async with BleakClient(esp32.address) as client:
            self.append_text("Connected. Streaming BLE messages...\n")
            await client.start_notify(CHARACTERISTIC_UUID, self.handle_ble_message)

            try:
                while True:
                    await asyncio.sleep(1)
            except Exception as e:
                self.append_text(f"Disconnected: {e}")
                await client.stop_notify(CHARACTERISTIC_UUID)

    def handle_ble_message(self, _, data: bytearray):
        try:
            message = data.decode("utf-8", errors="ignore").strip()
            timestamp = datetime.now().strftime("%H:%M:%S")

            if "Timing" in message:
                return

            formatted = f"[{timestamp}] [CAM]: {message}"

            if "Detected" in message:
                self.root.after(0, self.append_text, formatted)

                # Parse the object name and confidence
                try:
                    parts = message.split("Detected ")[1].split(" at")[0]
                    label = parts.split(" (")[0].strip()
                    confidence = float(parts.split("(")[1].rstrip(")"))
                    self.root.after(0, self.update_catalogue, label, confidence)
                except Exception as e:
                    print(f"Error parsing detection line: {e}")
            else:
                self.root.after(0, self.append_text, formatted)
        except Exception as e:
            self.root.after(0, self.append_text, f"[Decode Error]: {e}")

    def start_ble_loop(self):
        asyncio.set_event_loop(self.loop)
        self.loop.run_until_complete(self.run_ble_client())

# --- Run the GUI ---
if __name__ == "__main__":
    root = Tk()
    gui = BLEConsoleGUI(root)
    root.mainloop()
