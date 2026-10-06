name TEST WAVE
author Alexnex

# Up at 45 degrees while the button is down, down at 45 degrees otherwise.
# Blocks and spikes kill on any touch; the ground and the corridor's
# floor and ceiling are safe to slide on.
portal 1500 650 2 wave

# a floor block: go over it
block 2300 650 2 w=4 h=4
# a ceiling block: go under it
block 3100 -150 2 w=4 h=14
# a tall floor block: over it
block 3900 450 2 w=4 h=8
# a window 300 px tall
block 4700 -150 2 w=4 h=8
block 4700 550 2 w=4 h=6
# a spike standing on the ground: the wave is small enough to slide under it
spike 5400 750 2
# two spikes in the air
spike 5900 300 2
spike 6200 550 2

# gravity flips, whatever the height: the button now sends the wave down
gravity 7000 -200 2 up
gravity 7000 100 2 up
gravity 7000 400 2 up
gravity 7000 700 2 up
block 7900 550 2 w=4 h=6
block 8700 -150 2 w=4 h=10
gravity 9500 -200 2 down
gravity 9500 100 2 down
gravity 9500 400 2 down
gravity 9500 700 2 down

# back to a cube from any height: it keeps the wave's last direction
portal 10300 -200 2 cube
portal 10300 100 2 cube
portal 10300 400 2 cube
portal 10300 700 2 cube
spike 11800 750 2
