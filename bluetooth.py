import serial
import time

# Replace with the correct port for your OS:
# Windows example: 'COM3'
# Linux example: '/dev/rfcomm0'
PORT = '/dev/rfcomm0'
BAUD = 115200

try:
    ser = serial.Serial(PORT, BAUD, timeout=1)
    print(f"Connected to {PORT} at {BAUD} baud rate.")
except serial.SerialException as e:
    print(f"Error: {e}")
    exit(1)

try:
    while True:
        ser.write(b'Hello from Python!\n')
        time.sleep(1)

        if ser.in_waiting:
            line = ser.readline().decode('utf-8').strip()
            print(f"Received from ESP32: {line}")

except KeyboardInterrupt:
    print("Disconnected.")
    ser.close()
