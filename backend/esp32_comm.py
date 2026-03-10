import serial

ser = serial.Serial("COM15", 115200, timeout=1)

def read_line():
    try:
        line = ser.readline().decode(errors="ignore").strip()
        return line
    except:
        return None


def send_command(cmd):
    try:
        ser.write((cmd + "\n").encode())
        print("Sent command:", cmd)
    except:
        print("Failed to send command")
