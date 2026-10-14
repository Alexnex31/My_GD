name TEST MINI
author Alexnex

# The pink portal makes you mini: half the size, smaller jumps. The green
# one gives you your size back.
mini 1200 650 2 on

# a low tunnel with a spike: only a mini jump fits under the wall
block 2000 -500 2 w=20 h=23
spike 2500 800 1

# a mini jump, onto two blocks
block 3600 750 2
block 4100 750 2

# a mini wave climbs twice as steep: up and over a wall, down again
portal 4800 650 2 wave
block 5800 450 2 h=8
portal 7200 650 2 cube

# full size again, one last spike
mini 7800 650 2 off
spike 8600 750 2
