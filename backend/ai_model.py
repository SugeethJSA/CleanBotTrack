import random

def decide(distance):

    if distance < 20:
        return random.choice(["L", "R"])

    elif distance < 35:
        return "F"

    else:
        return random.choice(["F", "F", "L"])
