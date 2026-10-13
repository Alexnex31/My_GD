name TEST SPEED
author Alexnex

# A speed portal sets how fast you go, until the next one. Jumps stay as high
# and only cover more or less ground. The names are GD's: 0.5 1 2 3 4.

# normal: one spike
spike 1500 750 2

# slow (0.5): short jumps, two blocks to hop onto
speed 2200 650 2 0.5
block 3000 750 2
block 3600 750 2

# fast (2): three spikes in one jump
speed 4400 650 2 2
spike 5400 750 2
spike 5500 750 2
spike 5600 750 2

# very fast (3): four
speed 6600 650 2 3
spike 8000 750 2
spike 8100 750 2
spike 8200 750 2
spike 8300 750 2

# extremely fast (4): six, too wide at any other speed
speed 9600 650 2 4
spike 11400 750 2
spike 11500 750 2
spike 11600 750 2
spike 11700 750 2
spike 11800 750 2
spike 11900 750 2

# back to normal
speed 13500 650 2 1
spike 14500 750 2
