/*
** ALEXNEX PROJECT, 2026
** tests/fuzz/fuzz_parser.c
** File description:
** libFuzzer: random bytes as a level file, then 200 ticks of play (8.1)
*/

#include <stddef.h>
#include <stdint.h>

#include "sim/sim.h"

/*
** The parser reads files people download and edit by hand: whatever the
** bytes, loading them and playing a few seconds must never crash or trip
** ASan/UBSan. Seed it with levels/.
*/
int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size)
{
    sim_t s;

    if (sim_load_mem(&s, (const char *)data, size, "1", NULL) != 0)
        return 0;
    for (int t = 0; t < 200; t++)
        sim_tick(&s, (input_t){t % 20 < 10, t % 20 == 0});
    sim_free(&s);
    return 0;
}
