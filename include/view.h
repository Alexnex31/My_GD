/*
** ALEXNEX PROJECT, 2026
** view.h
** File description:
** header file for my_gd project
*/

#ifndef GD_VIEW_H
    #define GD_VIEW_H

/*
** Presentation constants (9.1). The logical screen the game draws in: the
** window can be any size, a letterbox viewport maps this onto it (9.7).
** These belong to the game layer, never to the simulation.
*/
    #define VIEW_W 1920.0f
    #define VIEW_H 1080.0f

    #define CHUNK_W 1024.0f       /* static vertex buffers, one per chunk (9.2) */

    #define BAR_W   700.0f        /* the progress bar at the top (9.5)          */
    #define BAR_H   36.0f
    #define END_FLIGHT 1.0f       /* seconds the player flies on at the end (9.8) */
    #define END_FLASH  0.3f      /* and the white flash after it */
    #define MAX_FLASHES 4        /* objects flashing as they are spent (9.2) */
    #define FLASH_TIME  0.2f

typedef enum draw_layer {         /* the draw order, in the game and the editor */
    LAYER_BLOCK, LAYER_HAZARD, LAYER_INTERACTIVE, LAYER_COUNT
} draw_layer_t;

#endif
