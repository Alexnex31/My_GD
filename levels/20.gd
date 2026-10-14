name TEST SPIDER
author Alexnex

# A click takes the spider straight to the surface above, and gravity turns
# round. Holding does nothing: click again to come back.
portal 1200 650 2 spider

# a floor of spikes: up to the ceiling, then down past it
spike 2000 750 2
spike 2100 750 2
spike 2200 750 2
spike 2300 750 2
spike 2400 750 2
spike 2500 750 2

# spikes on the ceiling: down again
spike 3200 -50 2 rot=180
spike 3300 -50 2 rot=180
spike 3400 -50 2 rot=180
spike 3500 -50 2 rot=180

# a block overhead: up onto its underside, over the spikes under it
block 4300 250 2 w=12 h=2
spike 4700 750 2
spike 4800 750 2
spike 4900 750 2

portal 6200 650 2 cube
spike 7000 750 2
