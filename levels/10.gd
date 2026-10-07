name TEST BALL
author Alexnex

# A click flips the gravity: the ball falls to the other surface.
# Holding flips again on every landing. A flip takes about 440 px of level.
portal 1500 650 2 ball

# floor, ceiling, floor: one flip each
spike 2400 750 2
spike 3200 50 2 rot=180
spike 4000 750 2

# spikes above and below: ride the platform, on it or under it
block 4700 400 2 w=12 h=1
spike 4900 750 2
spike 5000 750 2
spike 4900 50 2 rot=180
spike 5000 50 2 rot=180

# a wall on the floor, then one hanging from the ceiling
block 6200 650 2 h=4
block 7000 50 2 h=4

# gravity back to normal whatever the height, then a cube again
gravity 7700 50 2 down
gravity 7700 350 2 down
gravity 7700 650 2 down
portal 8100 50 2 cube
portal 8100 350 2 cube
portal 8100 650 2 cube
spike 9200 750 2
