name TEST SAWS
author Alexnex

# A saw kills on any contact. Its hitbox is a disc, a little inside its teeth.

# one to jump, then two in a row
saw 1500 750 2
saw 2500 750 2
saw 2600 750 2

# hanging: run under the first, jump between the pair
saw 3500 600 2
saw 4400 750 2
saw 4400 350 2

# a bigger one: jump late
saw 5400 700 3

# as a ship, through three pairs
portal 6800 650 2 ship
saw 7600 500 4
saw 7600 -100 4
saw 8600 200 4
saw 8600 700 4
saw 9600 450 4
saw 9600 -150 4
portal 10600 650 2 cube

saw 11400 750 2
