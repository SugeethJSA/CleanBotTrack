import numpy as np

grid = np.zeros((20,20))

robot_x = 10
robot_y = 10


def update_map(distance):

    global robot_x, robot_y

    if distance < 20:

        if robot_y + 1 < 20:
            grid[robot_x][robot_y+1] = 1

    else:

        if robot_y + 1 < 20:
            grid[robot_x][robot_y] = 2
            robot_y += 1

    print("\nMap:")
    print(grid)

    return grid
