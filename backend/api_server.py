from fastapi import FastAPI
from map_system import grid

app = FastAPI()

@app.get("/")
def home():
    return {"status":"Robot AI Server Running"}

@app.get("/map")
def get_map():
    return {"map": grid.tolist()}
