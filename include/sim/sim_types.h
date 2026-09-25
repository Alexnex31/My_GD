/*
** ALEXNEX PROJECT, 2026
** sim/sim_types.h
** File description:
** header file for my_gd project
*/

#ifndef MYGD_SIM_TYPES_H
    #define MYGD_SIM_TYPES_H
    #include <stdbool.h>
    #include <stddef.h>
    #include <stdint.h>

    #include "sim/constants.h"

typedef struct vec2 { double x; double y; } vec2_t;   /* the sim computes in double (below) */
typedef struct rect { double x; double y; double w; double h; } rect_t;
typedef void (*sim_log_fn)(const char *msg);

typedef struct input {
    bool held;                /* button down during this tick           */
    bool pressed;             /* first tick after a physical press      */
} input_t;

typedef enum obj_type {
    OBJ_BLOCK,                /* neutral: rectangle                          */
    OBJ_SLOPE,                /* neutral: right triangle (7.2)               */
    OBJ_SPIKE,                /* harm                                        */
    OBJ_PORTAL,               /* interactive: acts once, then untouchable    */
    /* later: OBJ_SAW (harm), OBJ_PAD, OBJ_ORB, OBJ_GRAVITY, OBJ_SPEED (FEATURES 10) */
    OBJ_TYPE_COUNT
} obj_type_t;

typedef enum obj_category { CAT_NEUTRAL, CAT_HARM, CAT_INTERACTIVE } obj_category_t;
extern const obj_category_t OBJ_CATEGORY[OBJ_TYPE_COUNT];

typedef enum gamemode { MODE_CUBE, MODE_SHIP, MODE_COUNT } gamemode_t;

typedef enum hold_state {     /* 3.4: GD's buffered clicks and orb locking */
    HOLD_NONE,                /* button up                                 */
    HOLD_FRESH,               /* down, hasn't produced a jump or orb yet   */
    HOLD_USED                 /* down, already used: jumps, but no orbs    */
} hold_state_t;

#define HB_MAX_VERTS 4

typedef enum shape_kind { SHAPE_POLY, SHAPE_CIRCLE } shape_kind_t;
typedef enum face_kind { FACE_HORIZONTAL, FACE_VERTICAL, FACE_TILTED } face_kind_t;

typedef struct hitbox {       /* full shape: area, not outline (3.0) */
    shape_kind_t kind;
    rect_t aabb;              /* world-space bounds: broadphase, first axes     */
    int nverts;               /* poly: 3 or 4, world space, clockwise on screen */
    vec2_t verts[HB_MAX_VERTS];
    int naxes;                /* poly: separating axes other than x and y       */
    vec2_t axes[HB_MAX_VERTS];     /* unit edge normals, pointing outward      */
    double axis_lo[HB_MAX_VERTS];   /* the shape's projection on each axis      */
    double axis_hi[HB_MAX_VERTS];
    uint8_t face_kind[HB_MAX_VERTS];   /* edge i = verts[i] -> verts[i + 1]:
                                          FACE_HORIZONTAL, FACE_VERTICAL, FACE_TILTED */
    vec2_t face_n[HB_MAX_VERTS];       /* edge i's outward unit normal (G.2)       */
    double face_off[HB_MAX_VERTS];     /* its line: dot(face_n, p) = face_off      */
    vec2_t center;            /* circle (FEATURES 10.6) */
    double radius;
} hitbox_t;

typedef struct object {       /* level data: never modified after load */
    obj_type_t type;
    rect_t rect;              /* unrotated bounds, world space, as in the file */
    double rotation;          /* degrees, clockwise, around the rect's center  */
    hitbox_t hitbox;          /* computed once at load, rotation included      */
    int size;                 /* as written in the file                        */
    gamemode_t portal_mode;   /* OBJ_PORTAL only                                */
    int line;                 /* source line, for messages and stable sort      */
} object_t;

typedef struct level_header { /* the level file's header fields (7.2) */
    char name[128];           /* the prose name; the id when the file has none  */
    char author[64];
    int version;              /* format version the file was written for        */
    char song[64];            /* a file in res/songs (FEATURES 4.5), "" = none  */
    double offset;            /* seconds of song skipped at the start           */
    double bpm;               /* the editor's beat grid (FEATURES 11.10)        */
    double first_beat;
} level_header_t;

typedef struct level_data {   /* immutable after sim_load */
    object_t *objects;        /* contiguous, sorted by hitbox.aabb.x (then line) */
    size_t nb_objects;
    double reach;              /* max over objects of hitbox.aabb.w: broadphase (4.1) */
    double end_shift;          /* distance at which the level completes (3.4)     */
    double kill_y;             /* kill ceiling, world y (4.7)                     */
    char id[24];               /* the file's digits, e.g. "10280" (7.2)           */
    level_header_t hdr;
} level_data_t;

typedef struct player {
    vec2_t pos;               /* center, world space; pos.x = spawn + distance */
    vec2_t prev_pos;          /* at the start of the tick (interpolation, 11.3) */
                              /* the three hitboxes share pos as their center (4.3) */
    double vx;                /* px per tick, to the right                      */
    double vy;                /* px per tick, rise speed (away from the floor)  */
    int gravity_dir;          /* +1 normal; -1 flipped (FEATURES 9)             */
    gamemode_t mode;
    bool grounded;            /* supported by a floor this tick                 */
    bool can_jump;            /* the jump zone test of the last tick (4.3)      */
    double surface_rise;      /* rise speed imposed by the supporting surface   */
    vec2_t support_normal;    /* that surface's normal (the icon lies along it) */
    float rotation;           /* the icon's angle, degrees, cosmetic (9.4)      */
    hold_state_t hold;        /* HOLD_NONE, HOLD_FRESH, HOLD_USED (3.4)         */
    bool alive;
} player_t;

typedef struct camera {
    vec2_t pos;               /* world position of the screen's top-left */
} camera_t;

typedef struct touch {        /* an interactive object met during the tick (4.6) */
    size_t index;
    double at;                /* path position: leg index + fraction along it     */
} touch_t;

typedef struct face {         /* one horizontal face of a hitbox (4.4, G.7) */
    double y;
    double x0;
    double x1;
} face_t;

typedef struct ship_bounds {
    bool active;
    double top;               /* world y of the ceiling surface */
    double bottom;            /* world y of the floor surface   */
} ship_bounds_t;

typedef struct run_state {    /* everything an attempt changes */
    long tick;
    double distance;          /* px scrolled since spawn (3.2)  */
    double speed_mult;        /* 1 until speed portals          */
    player_t player;
    camera_t cam;
    ship_bounds_t bounds;
    size_t first_active;      /* broadphase cursor (4.1)        */
    bool complete;
    uint64_t *spent;          /* bitset, one bit per object (5.1) */
    size_t spent_words;
} run_state_t;

typedef struct sim_snapshot {
    run_state_t st;           /* st.spent points into the snapshot's own buffer */
} sim_snapshot_t;

#define SURF_GROUND (-1L)
#define SURF_FLOOR  (-2L)          /* the corridor's floor   */
#define SURF_CEIL   (-3L)          /* the corridor's ceiling */

static inline bool is_surface(long s) { return s < 0; }
typedef struct contact {
    double t;            /* fraction of the move, 0..1                        */
    vec2_t normal;       /* unit, out of the obstacle toward the player       */
    bool flat;           /* the square met the shape's top or bottom (y axis) */
    double offset;       /* the face's line: normal . point = offset          */
    long surface;        /* object index, or SURF_GROUND / SURF_CEIL / SURF_FLOOR */
} contact_t;

typedef enum event_kind { EV_NONE, EV_CONTACT, EV_STEP } event_kind_t;

typedef struct event {   /* what cuts a leg of the move short (4.4) */
    event_kind_t kind;
    double t;            /* fraction of the leg, 0..1; 1 with EV_NONE */
    contact_t contact;   /* EV_CONTACT */
    face_t step;         /* EV_STEP: the face to climb onto          */
} event_t;

typedef struct sim {                  /* the run state, plus this tick's scratch space */
    level_data_t lvl;
    run_state_t st;
    size_t cand[MAX_CANDIDATES];      /* broadphase result, object indices (4.1)     */
    size_t nb_cand;
    size_t passed[MAX_CANDIDATES];    /* objects passed into as walls this tick (4.4) */
    size_t nb_passed;
    touch_t touch[MAX_TOUCHES];       /* live interactive objects touched (4.6)      */
    size_t nb_touch;
    int legs;                         /* legs advanced this tick: path positions     */
} sim_t;

static inline bool is_spent(const run_state_t *st, size_t i)
{
    return (st->spent[i / 64] >> (i % 64)) & 1u;
}

static inline void set_spent(run_state_t *st, size_t i)
{
    st->spent[i / 64] |= (uint64_t)1 << (i % 64);
}

#endif
