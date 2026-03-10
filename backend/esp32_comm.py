import serial

ser = serial.Serial("COM15", 115200, timeout=1)

def read_sensor():

    line = ser.readline().decode(errors="ignore").strip()

    if "DIST:" in line:
        try:
            dist = int(line.split(":")[1])
            return dist
        except:
            return None

    return None


def send_command(cmd):
    ser.write((cmd + "\n").encode())
