from fastapi import FastAPI
from esp32_comm import send_command
from map_system import grid, robot_state

app = FastAPI()


@app.get("/")
def home():
    return {"status": "Robot Server Running"}


@app.get("/state")
def get_state():
    return robot_state


@app.get("/map")
def get_map():
    return {"map": grid.tolist()}


@app.post("/forward")
def forward():
    send_command("F")
    robot_state["last_command"] = "F"
    return {"status":"ok"}


@app.post("/back")
def back():
    send_command("B")
    robot_state["last_command"] = "B"
    return {"status":"ok"}


@app.post("/left")
def left():
    send_command("L")
    robot_state["last_command"] = "L"
    return {"status":"ok"}


@app.post("/right")
def right():
    send_command("R")
    robot_state["last_command"] = "R"
    return {"status":"ok"}


@app.post("/stop")
def stop():
    send_command("S")
    robot_state["last_command"] = "S"
    return {"status":"ok"}


@app.post("/vacuum/on")
def vacuum_on():
    send_command("V1")
    return {"status":"ok"}


@app.post("/vacuum/off")
def vacuum_off():
    send_command("V0")
    return {"status":"ok"}
