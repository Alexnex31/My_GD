name TEST ROBOT
author Alexnex

# The robot rises for as long as you hold, up to 3.5 blocks, then falls.
# A tap is a small hop.
portal 1200 650 2 robot

# a tap: one spike
spike 2000 750 2

# held a little: two blocks
block 3000 750 2 h=2

# held all the way: a wall three blocks tall, too high for a cube
block 4200 550 2 h=6

# short hops only: spikes under a low ceiling, too thick to jump onto
block 5600 0 2 w=16 h=12
spike 6000 750 2
spike 6700 750 2

# held again: onto a tower, then a long drop
block 7800 550 2 w=4 h=6
spike 8500 750 2

portal 9400 650 2 cube
spike 10200 750 2
