/*
** ALEXNEX PROJECT, 2026
** sim/constants.h
** File description:
** header file for my_gd project
*/

#ifndef MYGD_CONSTANTS_H
    #define MYGD_CONSTANTS_H

    #define TICK_RATE           240
    #define PER_TICK(v)         ((double)(v) / TICK_RATE)
    #define PER_TICK2(a)        ((double)(a) / ((double)TICK_RATE * TICK_RATE))

    /* World */
    #define UNIT                50.0
    #define GROUND_Y            850.0
    #define LEVEL_END_PADDING   500.0   /* level ends 500 px after the last object */
    #define KILL_CEILING_MARGIN 600.0    /* flipped gravity only: kill line above the highest object (4.7) */

    /* Player */
    #define PLAYER_HALF         50.0    /* rigid square 100x100, circle radius 50 (4.3) */
    #define PLAYER_SCREEN_X     350.0
    #define PLAYER_SPAWN_X      350.0
    #define PLAYER_SPAWN_Y      (GROUND_Y - PLAYER_HALF)   /* 800, on the ground */

    /* Motion: GD's own numbers (FEATURES 6.9). GD counts in blocks (1 block = 1 player
       = 100 px here) and writes every vertical speed relative to the horizontal one:
       1 velocity unit = the normal scroll speed, 1 acceleration unit = that speed
       squared per block. Constants marked "ours" are values GD doesn't publish. */
    #define BLOCK               100.0    /* one GD block: the player's size              */
    #define SPEED_SLOW          (8.372 * BLOCK)    /* GD's five speeds, written 0.5, 1,  */
    #define SPEED_NORMAL        (10.386 * BLOCK)   /* 2, 3 and 4 in level files (10.3)   */
    #define SPEED_FAST          (12.914 * BLOCK)
    #define SPEED_VFAST         (15.6 * BLOCK)
    #define SPEED_XFAST         (19.2 * BLOCK)
    #define SCROLL_SPEED        SPEED_NORMAL       /* 1038.6 px/s, 4.3275 px/tick        */
    #define V_UNIT              SPEED_NORMAL       /* 1 GD velocity unit                 */
    #define A_UNIT              (SPEED_NORMAL * SPEED_NORMAL / BLOCK)  /* 10786.9 px/s^2 */

    #define CUBE_GRAVITY        (0.876 * A_UNIT)   /* 9449.3 px/s^2                      */
    #define CUBE_JUMP_V         (1.9522 * V_UNIT)  /* 2027.6 px/s: GD's 1.94 raised so the
                                                    measured apex is its 2.1333 blocks (11.1) */
    #define CUBE_MAX_FALL       (2.6 * V_UNIT)     /* 2700.4 px/s                        */
    #define CUBE_SPIN           324      /* deg/s, the icon's spin in the air (cosmetic) */
    #define RISE_EPSILON        (1.0 / 4096.0)   /* px/tick, jump-zone momentum test (4.3) */
    #define SHIP_GRAVITY        (0.99 * A_UNIT)    /* ours: 10678.9 px/s^2               */
    #define SHIP_THRUST         (2.24 * A_UNIT)    /* ours: 24162.7 px/s^2               */
    #define SHIP_MAX_VY         (2.24 * V_UNIT)    /* ours: 2326.5 px/s                  */

    /* Contacts (4.4) */
    #define FLOOR_MIN_DOT       0.64279 /* cos(50 deg): steeper than 50 deg is a wall     */
    #define FLOOR_MAX_TAN       1.19175 /* tan(50 deg)                                    */
    #define PLAYER_INNER_HALF   20.0    /* 40x40 inner box: a neutral touch kills (4.3) */
    #define BOUNCE_RESTITUTION_SHIP 0.3 /* share of the rise speed kept by a ceiling bounce */
    #define BOUNCE_MIN_SPEED    60       /* px/s: slower hits just stop (no micro-bounces) */
    #define MAX_CONTACTS        4        /* contacts resolved per tick                     */
    #define MAX_CANDIDATES      256      /* objects a tick's broadphase may collect (4.1)  */
    #define MAX_TOUCHES         32       /* interactive objects touched in one tick (4.6)  */
    #define CONTACT_SKIN        (1.0 / 1024.0)   /* gap kept from tilted faces (4.4)     */

    /* Camera (screen y of the player's center) */
    #define CAM_TOP_MARGIN      200.0
    #define CAM_BOTTOM_MARGIN   790.0
    #define CAM_TAU             0.08    /* seconds, smoothing time constant (3.5)          */
    #define CAM_LERP            0.050750 /* = 1 - exp(-1 / (TICK_RATE * CAM_TAU))          */

    /* Ship corridor (5.2) */
    #define CORRIDOR_MAX_HEIGHT 1000.0   /* the tallest mode corridor (ship, UFO, wave) */
    #define VIEW_HEIGHT         1080.0   /* logical screen height the camera reasons in (9.1) */

    /* Death */
    #define DEATH_DELAY_TICKS   (TICK_RATE / 2)   /* 0.5 s */

#endif
