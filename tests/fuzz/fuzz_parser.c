/*
** ALEXNEX PROJECT, 2026
** tests/fuzz/fuzz_parser.c
** File description:
** libFuzzer: random bytes as a level file, saved and read back, then played (8.1)
*/

#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "sim/level.h"
#include "sim/sim.h"

/*
** What the editor would save from these bytes, read back and saved again,
** is the same text: a save loses or changes nothing it wrote (7.2).
*/
static void save_twice(const uint8_t *data, size_t size)
{
    level_doc_t doc;
    level_header_t hdr;
    char *first;
    char *second;

    level_header_defaults(&hdr, "1");
    level_parse_doc((const char *)data, size, "1", &doc, &hdr, NULL);
    first = level_write_mem(&doc, &hdr);
    level_doc_free(&doc);
    level_header_defaults(&hdr, "1");
    level_parse_doc(first, strlen(first), "1", &doc, &hdr, NULL);
    second = level_write_mem(&doc, &hdr);
    level_doc_free(&doc);
    if (strcmp(first, second) != 0)
        abort();
    free(first);
    free(second);
}

static void play(sim_t *s)
{
    for (int t = 0; t < 200; t++)
        sim_tick(s, (input_t){t % 20 < 10, t % 20 == 0});
}

/*
** The parser reads files people download and edit by hand: whatever the
** bytes, loading them, saving them and playing a few seconds from every
** start must never crash or trip ASan/UBSan. Seed it with levels/.
*/
int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size)
{
    sim_t s;

    save_twice(data, size);
    if (sim_load_mem(&s, (const char *)data, size, "1", NULL) != 0)
        return 0;
    play(&s);
    for (size_t i = 0; i < s.lvl.nb_starts && i < 4; i++) {
        sim_reset_at(&s, &s.lvl.starts[i]);
        play(&s);
    }
    sim_free(&s);
    return 0;
}
