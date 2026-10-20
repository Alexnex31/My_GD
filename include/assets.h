/*
** ALEXNEX PROJECT, 2026
** assets.h
** File description:
** header file for my_gd project
*/

#ifndef ASSETS_H
    #define ASSETS_H

/*
** Every file the game loads from res/, in one place (README "Assets"):
**   fonts/                   the text's
**   menus/backgrounds/       one per screen
**   menus/buttons/           the image buttons (button.c)
**   menus/cursor.png
**   level/                   the world: background, ground, objects, death
**   icons/<mode>/            the player's icons, per gamemode
**   sounds/                  not played yet (README)
** A file that nothing loads yet keeps its place: its name is the slot its
** real art will fill.
*/
    #define ASSET_FONT "res/fonts/GDfont.ttf"

    #define ASSET_BG_MAIN "res/menus/backgrounds/main_background.png"
    #define ASSET_BG_LEVEL_LIST "res/menus/backgrounds/level_list_background.png"
    #define ASSET_BG_OPTIONS "res/menus/backgrounds/noe_background.jpeg"
    #define ASSET_BG_EDITOR "res/menus/backgrounds/cecilya.png"
    #define ASSET_BG_END_LEVEL "res/menus/backgrounds/end_level_background.png"

    #define ASSET_BUTTON_PLAY "res/menus/buttons/play_button.png"
    #define ASSET_BUTTON_OPTIONS "res/menus/buttons/param_button.png"
    #define ASSET_BUTTON_EDITOR "res/menus/buttons/online_button.png"
    #define ASSET_BUTTON_RETRY "res/menus/buttons/retry_button.png"
    #define ASSET_BUTTON_QUIT "res/menus/buttons/quit_button.png"
    #define ASSET_CURSOR "res/menus/cursor.png"

    #define ASSET_LEVEL_BG "res/level/level_background.png"
    #define ASSET_GROUND "res/level/ground.png"
    #define ASSET_BLOCK "res/level/block.png"
    #define ASSET_SPIKE "res/level/spike.png"
    #define ASSET_PORTAL "res/level/cube_portal.png"
    #define ASSET_EXPLOSION "res/level/explos.png"

    #define ASSET_ICON_CUBE "res/icons/cube/player_icon.png"
    #define ASSET_ICON_SHIP "res/icons/ship/ship_icon.png"

#endif
