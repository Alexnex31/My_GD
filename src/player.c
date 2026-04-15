/*
** ALEXNEX PROJECT, 2026
** player
** File description:
** functions to create and manage a player
*/

#include "mygd.h"

void free_player(player_t *player)
{
    if (player != NULL) {
        if (player->sprite != NULL)
            sfSprite_destroy(player->sprite);
        free(player);
    }
}

player_t *create_player(gd_t *gd)
{
    player_t *player = malloc(sizeof(player_t));

    player->sprite = sfSprite_create();
    sfSprite_setTexture(player->sprite, gd->res->player_icon, sfTrue);
    sfSprite_setOrigin(player->sprite, (sfVector2f){50, 50});
    player->pos = (sfVector2f){400, 750};
    sfSprite_setPosition(player->sprite, player->pos);
    player->vy = 0;
    player->size = 2;
    player->orientation = 0;
    player->gamemode = 'c';
    player->state = 'a';
    player->allow_jump = 'n';
    return player;
}
