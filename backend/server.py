import time

from esp32_comm import read_line, send_command
from ai_model import decide
from map_system import update_map, robot_state

autonomous_mode = False

print("Robot Server Running")


def process_sensor_line(line):

    if "DIST:" in line:
        dist = int(line.split(":")[1])
        robot_state["distance_ultra"] = dist
        update_map(dist)
        return dist

    if "IR:" in line:
        robot_state["distance_ir"] = int(line.split(":")[1])

    if "COLOR:" in line:
        robot_state["color"] = line.split(":")[1]

    return None


while True:

    line = read_line()

    if not line:
        continue

    print("Received:", line)

    dist = process_sensor_line(line)

    if autonomous_mode and dist is not None:

        cmd = decide(dist)

        if cmd:
            send_command(cmd)
            robot_state["last_command"] = cmd

    time.sleep(0.1)
