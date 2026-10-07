name TEST PADS
author Alexnex

# A pad launches on touch, the same height whatever you were doing.

# yellow: 4.4 blocks, over a wall no jump clears
pad 1400 750 2 yellow
block 1700 450 2 h=8

# pink: 1.8 blocks, a low hop over two spikes
pad 2700 750 2 pink
spike 2780 750 2
spike 2880 750 2

# red: 7.6 blocks, onto a tower
pad 3700 750 2 red
block 4200 150 4 h=14

# blue: gravity flips and you are thrown at the ceiling block.
# Jump off it over the hanging spike; the hanging blue pad sends you back.
pad 5600 750 2 blue
block 5700 250 2 w=16 h=2
spike 6000 350 2 rot=180
pad 6300 350 2 blue rot=180

spike 7400 750 2
