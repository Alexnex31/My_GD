name TEST ORBS
author Alexnex

# An orb acts when you click while touching it. Holding from before works
# too, if that hold has not jumped yet. One click, one orb.

# yellow: six spikes are too wide for a jump, click in the air
spike 1500 750 2
spike 1600 750 2
spike 1700 750 2
spike 1800 750 2
spike 1900 750 2
spike 2000 750 2
orb 1750 550 2 yellow

# pink: a smaller lift, enough for four spikes
spike 3000 750 2
spike 3100 750 2
spike 3200 750 2
spike 3300 750 2
orb 3150 600 2 pink

# red: 4.1 blocks from where you click, onto a wall
orb 4250 600 2 red
block 4500 350 4 h=10

# blue: gravity flips, you are thrown onto the ceiling block, over a floor
# of spikes. Green, past its end, flips you back with a jump.
orb 5700 600 2 blue
block 5800 200 2 w=16 h=2
spike 5900 750 2
spike 6000 750 2
spike 6100 750 2
spike 6200 750 2
spike 6300 750 2
orb 6650 300 2 green

# black: the yellow pad throws you at a hanging wall, the orb slams you
# back down under it
pad 7400 750 2 yellow
orb 7700 320 2 black
block 7900 100 2 h=10

spike 8800 750 2
