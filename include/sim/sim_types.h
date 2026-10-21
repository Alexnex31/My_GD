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
    OBJ_GRAVITY,              /* interactive: a gravity portal (FEATURES 9.4) */
    OBJ_PAD,                  /* interactive: launches on touch (FEATURES 10.1) */
    OBJ_ORB,                  /* interactive: launches on a fresh hold (10.2)  */
    OBJ_SAW,                  /* harm: a disc (FEATURES 10.6)                  */
    OBJ_SPEED,                /* interactive: a speed portal (FEATURES 10.3)   */
    OBJ_MINI,                 /* interactive: a size portal (FEATURES 10.4)    */
    OBJ_TYPE_COUNT
} obj_type_t;

typedef enum launch_kind {    /* the colours of pads and orbs (FEATURES 10.1, 10.2) */
    LAUNCH_YELLOW, LAUNCH_PINK, LAUNCH_RED, LAUNCH_BLUE,
    LAUNCH_GREEN, LAUNCH_BLACK,                 /* orbs only */
    LAUNCH_KIND_COUNT
} launch_kind_t;

typedef enum obj_category { CAT_NEUTRAL, CAT_HARM, CAT_INTERACTIVE } obj_category_t;
extern const obj_category_t OBJ_CATEGORY[OBJ_TYPE_COUNT];

/*
** GD's Z layers (FEATURES 11.15): B5 to B1 are drawn behind the player, T1
** to T4 in front of it, each over the one before. T1 is 0, so an object
** made in code is on the layer of everything with a hitbox.
*/
typedef enum z_layer {
    Z_B5 = -5, Z_B4, Z_B3, Z_B2, Z_B1, Z_T1, Z_T2, Z_T3, Z_T4,
    Z_LAYER_FIRST = Z_B5, Z_LAYER_LAST = Z_T4
} z_layer_t;

/* T1 for what has a hitbox the player meets, B1 for a picture only. */
z_layer_t obj_default_z_layer(obj_type_t type);
/* "b5" .. "t4", as a level file writes it. */
const char *z_layer_name(z_layer_t z);

typedef enum gamemode { MODE_CUBE, MODE_SHIP, MODE_UFO, MODE_WAVE, MODE_BALL, MODE_ROBOT, MODE_SWING, MODE_SPIDER, MODE_COUNT } gamemode_t;

typedef enum hold_state {     /* 3.4: GD's buffered clicks and orb locking */
    HOLD_NONE,                /* button up                                 */
    HOLD_FRESH,               /* down, hasn't produced a jump or orb yet   */
    HOLD_USED                 /* down, already used: jumps, but no orbs    */
} hold_state_t;

#define MAX_DEBUG_EVENTS 8
#define HB_MAX_VERTS 4
#define HB_MAX_AXES (HB_MAX_VERTS + 2)   /* y and x, then the shape's own (G.2) */

typedef enum shape_kind { SHAPE_POLY, SHAPE_CIRCLE } shape_kind_t;
typedef enum face_kind { FACE_HORIZONTAL, FACE_VERTICAL, FACE_TILTED } face_kind_t;

typedef struct hitbox {       /* full shape: area, not outline (3.0) */
    shape_kind_t kind;
    rect_t aabb;              /* world-space bounds: broadphase, first axes     */
    int nverts;               /* poly: 3 or 4, world space, clockwise on screen */
    vec2_t verts[HB_MAX_VERTS];
    int naxes;                /* separating axes: [0] is y, [1] is x, then its own */
    vec2_t axes[HB_MAX_AXES];       /* unit edge normals, pointing outward      */
    double axis_lo[HB_MAX_AXES];    /* the shape's projection on each axis      */
    double axis_hi[HB_MAX_AXES];
    double axis_extent[HB_MAX_AXES];   /* |a.x| + |a.y|: a square's radius on it */
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
    int portal_gravity;       /* OBJ_GRAVITY only: +1 down, -1 up               */
    launch_kind_t launch;     /* OBJ_PAD, OBJ_ORB: its colour                   */
    double speed;             /* OBJ_SPEED only: its name, 0.5 to 4 (10.3)      */
    bool portal_mini;         /* OBJ_MINI only: on makes the player mini (10.4) */
    z_layer_t z_layer;        /* behind or in front of the player (11.15)       */
    int z_order;              /* drawn over the lower ones of its Z layer       */
    int editor_layer;         /* the editor's only: the layer it is shown in    */
    int line;                 /* source line: placement order, messages, sort   */
} object_t;

/*
** The draw order, the game's and the editor's: by Z layer, then Z order,
** then the order they were placed in (`placed_a`, `placed_b`), the later
** on top. < 0 when a is drawn under b.
*/
int object_draw_cmp(const object_t *a, long placed_a, const object_t *b,
    long placed_b);

/*
** Two objects the game can't tell apart: place, kind, settings, Z. One of
** them is invisible and only costs collision time: the editor won't place
** it, and the level checks report it (7.4, 11.13g).
*/
bool object_same(const object_t *a, const object_t *b);

typedef struct level_start {  /* where and how an attempt starts (7.2) */
    vec2_t pos;               /* start_x, start_y: the player's center           */
    gamemode_t mode;          /* start_gamemode                                  */
    double speed;             /* start_speed: GD's name for it, 0.5, 1, 2, 3 or 4:
                                 a name, not a ratio (FEATURES 10.3)             */
    int gravity_dir;          /* start_gravity: +1 normal, -1 flipped            */
    bool mini;                /* start_size: the player starts mini (FEATURES 10.4) */
} level_start_t;

typedef struct level_header { /* the level file's header fields (7.2) */
    char name[128];           /* the prose name; the id when the file has none  */
    char author[64];
    char music[64];           /* a file in music/ (FEATURES 4.5), "" = none     */
    double music_offset;      /* seconds of song skipped at the start           */
    bool music_offset_set;    /* false: the song's default_offset (4.4)         */
    double bpm;               /* the editor's beat grid (FEATURES 11.11)        */
    double first_beat;        /* song time of beat 0, with bpm (FEATURES 11.11) */
    level_start_t start;
} level_header_t;

/*
** Where a run changes speed, for what needs the time an x is reached at
** (sim/speed.h): the player's center when it takes the portal, the portal's
** far side, and the px per tick from there on.
*/
typedef struct speed_change {
    double x;
    double right;
    double step;
    size_t order;             /* when it was added: ties in x (FEATURES 10.3) */
} speed_change_t;

typedef struct speed_map {
    speed_change_t *at;       /* sorted by x once speed_map_sort has run */
    size_t n;
    size_t cap;
} speed_map_t;

typedef struct level_data {   /* immutable after sim_load */
    object_t *objects;        /* contiguous, sorted by hitbox.aabb.x (then line) */
    size_t nb_objects;
    double reach;              /* max over objects of hitbox.aabb.w: --check (4.1) */
    double end_shift;          /* distance at which the level completes (3.4)     */
    double kill_y;             /* kill ceiling, world y (4.7)                     */
    level_start_t *starts;     /* the level's other start positions, in file order:
                                  practice only, never a completion (FEATURES 11.8) */
    size_t nb_starts;
    speed_map_t speeds;        /* its speed portals, in x order (FEATURES 10.3)   */
    int skipped_lines;         /* object lines the loader dropped (7.4)           */
    uint64_t file_hash;        /* fnv1a of the bytes loaded: the version (6.4)    */
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
    int boost_ticks;          /* the robot's rise: ticks of it left (10.5)      */
    bool mini;                /* half size, with its own numbers (10.4)         */
    bool teleport;            /* the spider's press: cross to the surface opposite
                                 in this tick's move (10.5). False between ticks */
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
    double speed_mult;        /* the speed over the normal one: set by the start
                                 and by speed portals (10.3)   */
    player_t player;
    camera_t cam;
    ship_bounds_t bounds;
    size_t first_active;      /* broadphase cursor (4.1)        */
    bool complete;
    uint64_t *spent;          /* bitset, one bit per object (5.1) */
    size_t spent_words;
    uint64_t spent_hash;      /* the set's hash, kept as bits are set: hashing
                                 a state never reads the whole bitset (8.3)  */
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
    bool inside;         /* the circle was already in the shape (4.4)          */
} contact_t;

typedef enum event_kind { EV_NONE, EV_CONTACT, EV_STEP } event_kind_t;

typedef struct event {   /* what cuts a leg of the move short (4.4) */
    event_kind_t kind;
    double t;            /* fraction of the leg, 0..1; 1 with EV_NONE */
    contact_t contact;   /* EV_CONTACT */
    face_t step;         /* EV_STEP: the face to climb onto          */
} event_t;

/*
** What the tick did, for the F3 overlay (9.6). Scratch, never run state: it is
** out of the hash, out of snapshots and out of replays, and NOTHING under
** src/sim/ may read it back -- the moment physics depends on it, the run stops
** being reproducible.
*/
typedef enum debug_kind {
    DBG_LAND, DBG_HEAD, DBG_PASS, DBG_STEP, DBG_DEATH
} debug_kind_t;

typedef struct debug_event {
    vec2_t pos;               /* the player's center when it happened        */
    vec2_t normal;            /* the contact's normal; (0,0) for a step      */
    debug_kind_t kind;
} debug_event_t;

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
    double tick_x0;                   /* distance when the tick started (3.2)        */
    double tick_left;                 /* the fraction of the tick still to travel    */
    debug_event_t dbg[MAX_DEBUG_EVENTS];   /* what this tick did, for 9.6 only       */
    int nb_dbg;
} sim_t;

static inline bool is_spent(const run_state_t *st, size_t i)
{
    return (st->spent[i / 64] >> (i % 64)) & 1u;
}

/* A key of its own for each object: their XOR is the set's hash. */
static inline uint64_t spent_key(size_t i)
{
    uint64_t z = (uint64_t)i + 0x9e3779b97f4a7c15ULL;

    z = (z ^ (z >> 30)) * 0xbf58476d1ce4e5b9ULL;
    z = (z ^ (z >> 27)) * 0x94d049bb133111ebULL;
    return z ^ (z >> 31);
}

static inline void set_spent(run_state_t *st, size_t i)
{
    uint64_t bit = (uint64_t)1 << (i % 64);

    if (st->spent[i / 64] & bit)
        return;
    st->spent[i / 64] |= bit;
    st->spent_hash ^= spent_key(i);
}

#endif
