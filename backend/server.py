import time

from esp32_comm import read_sensor, send_command
from ai_model import decide
from map_system import update_map


print("Robot AI Controller Running")

while True:

    dist = read_sensor()

    if dist is None:
        continue

    print("Distance:", dist)

    cmd = decide(dist)

    print("Command:", cmd)

    send_command(cmd)

    update_map(dist)

    time.sleep(0.2)
