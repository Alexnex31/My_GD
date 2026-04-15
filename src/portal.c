#include "mygd.h"

void load_portal(char **arr, level_t *level, gd_t *gd)
{
    portal_t *portal = malloc(sizeof(portal_t));

    portal->pos = (sfVector2f){atof(arr[1]), atof(arr[2])};
    if (atof(arr[1]) + 500 > level->level_end)
        level->level_end = atof(arr[1]) + 500;
    portal->size = atoi(arr[3]);
    if (strcmp(arr[4], "cube") == 0)
        portal->gamemode = 'c';
    if (strcmp(arr[4], "ship") == 0)
        portal->gamemode = 'p';

    portal->sprite = sfSprite_create();
    if (portal->gamemode == 'c')
        sfSprite_setTexture(portal->sprite, gd->res->player_icon, sfTrue);
    if (portal->gamemode == 'p')
        sfSprite_setTexture(portal->sprite, gd->res->ship_icon, sfTrue);
    sfSprite_setPosition(portal->sprite, portal->pos);
    level->objects->nb_portals += 1;
    level->objects->portals = realloc(level->objects->portals, sizeof(portal_t *) * (level->objects->nb_portals + 1));
    level->objects->portals[level->objects->nb_portals - 1] = portal;
    level->objects->portals[level->objects->nb_portals] = NULL;
    return;
}
