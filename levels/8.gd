name TEST UFO
author Alexnex

# One press = one hop, on the ground or in the air. Holding does nothing.
portal 1500 650 2 ufo

# a low wall: one hop
block 2400 750 2
# two spikes: one hop over both
spike 3000 750 2
spike 3100 750 2
# a 300 px wall: two hops, the second near the top of the first
block 3900 550 2 h=6
# a window 300 px tall between two walls
block 4900 550 2 h=6
block 4900 -150 2 h=8
# a low tunnel: hop over the spikes without bonking the ceiling too hard
block 5700 450 2 w=14 h=2
spike 5950 750 2
spike 6050 750 2

# gravity flips: the UFO falls up to the corridor's ceiling, hops go down
gravity 6900 600 2 up
spike 8200 -150 2 rot=180
spike 8300 -150 2 rot=180
# a hanging wall, 200 px: two hops
block 9000 -150 2 h=4
gravity 9800 -100 2 down

# back to a cube, one last jump
portal 11000 650 2 cube
spike 11800 750 2
