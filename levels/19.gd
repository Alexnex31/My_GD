name TEST SWING
author Alexnex

# Each click turns the swing's gravity round. Holding does nothing: click
# again to come back.
portal 1200 650 2 swing

# up: a wall from the floor
block 2200 350 2 h=10

# down: a wall from the ceiling
block 3400 -150 2 h=10

# spikes on the floor, on the ceiling, on the floor
spike 4600 750 2
spike 5200 -150 2 rot=180
spike 5800 750 2

# a tunnel in the middle: short swings, up and down
block 6800 -150 2 w=16 h=8
block 6800 450 2 w=16 h=8

portal 8200 650 2 cube
spike 9000 750 2
