/*
** ALEXNEX PROJECT, 2026
** level_render
** File description:
** drawing a level: views, camera, objects, the player (9.1, 9.4)
*/

#include "mygd.h"
#include "sim/modes.h"

#define STRIP_H     100.0f        /* the corridor's floor and ceiling (5.3)   */
#define FADE_TIME   0.25f         /* how long a removed corridor lingers      */
#define EXPLOSION_FRAMES 3

/* A whole screen pixel, so tiles don't shimmer on a fractional camera (9.1). */
static float snap(gd_t *gd, double v)
{
    float scale = gd->viewport_px_w / VIEW_W;

    if (scale <= 0.0f)
        return (float)v;
    return roundf((float)v * scale) / scale;
}

/*
** Where the player is drawn: between its last two ticks, by the fraction of
** a tick still in the accumulator, so no frame shows a stepped position
** (11.3). Only while ticks run: dying or finished, the sim stands still and
** the accumulator would only make it wobble. Presentation only.
*/
static vec2_t drawn_player_pos(const level_t *lv)
{
    const player_t *p = &lv->sim.st.player;
    double alpha = (double)lv->accumulator / 1000000.0;

    if (lv->state != LEVEL_PLAYING)
        return p->pos;

    return (vec2_t){p->prev_pos.x + (p->pos.x - p->prev_pos.x) * alpha,
        p->prev_pos.y + (p->pos.y - p->prev_pos.y) * alpha};
}

/* The camera follows the drawn player on x, so the two never drift (11.3). */
vec2_t level_camera(gd_t *gd, const level_t *lv)
{
    return (vec2_t){snap(gd, drawn_player_pos(lv).x - PLAYER_SCREEN_X),
        snap(gd, lv->sim.st.cam.pos.y)};
}

/*
** A corridor boundary: the block texture repeated across the view, in world
** space, so it scrolls with the level (5.3).
*/
static void draw_strip(gd_t *gd, level_t *lv, float cam_x, float y,
    sfUint8 alpha)
{
    sfSprite *s = lv->strip_sprite;
    float x = floorf(cam_x / 100.0f) * 100.0f;

    sfSprite_setTextureRect(s, (sfIntRect){0, 0, (int)VIEW_W + 200,
        (int)STRIP_H});
    sfSprite_setColor(s, (sfColor){255, 255, 255, alpha});
    sfSprite_setPosition(s, (sfVector2f){x, y});
    draw_sprite(gd, s, NULL);
}

static void draw_bounds(gd_t *gd, level_t *lv, const ship_bounds_t *b,
    float cam_x, sfUint8 alpha)
{
    draw_strip(gd, lv, cam_x, (float)b->top - STRIP_H, alpha);
    if (b->bottom < GROUND_Y)
        draw_strip(gd, lv, cam_x, (float)b->bottom, alpha);
}

/*
** The collision of an old corridor is gone at once (5.2), but its strips fade
** out over a quarter second. Purely visual: the sim never sees them.
*/
static void render_ship_bounds(gd_t *gd, level_t *lv, float cam_x)
{
    const ship_bounds_t *now = &lv->sim.st.bounds;

    if (lv->drawn_bounds.active && (!now->active
        || now->top != lv->drawn_bounds.top)) {
        lv->fading_bounds = lv->drawn_bounds;
        lv->fade_left = FADE_TIME;
    }
    lv->drawn_bounds = *now;
    if (lv->fade_left > 0.0f) {
        draw_bounds(gd, lv, &lv->fading_bounds, cam_x,
            (sfUint8)(255.0f * lv->fade_left / FADE_TIME));
        lv->fade_left -= (float)lv->frame_us / 1000000.0f;
    }
    if (now->active)
        draw_bounds(gd, lv, now, cam_x, 255);
}

static void start_flash(level_t *lv, size_t index)
{
    for (int i = 0; i < MAX_FLASHES; i++)
        if (lv->flash_left[i] <= 0.0f) {
            lv->flash_index[i] = index;
            lv->flash_left[i] = FLASH_TIME;
            return;
        }
}

/*
** An interactive object keeps its sprite when it fires (5.1); the feedback is
** its white silhouette fading out over it, all of them in one draw call.
*/
static void render_spent_flashes(gd_t *gd, level_t *lv)
{
    const run_state_t *st = &lv->sim.st;
    float dt = (float)lv->frame_us / 1000000.0f;
    sfRenderStates rs = {sfBlendAlpha, sfTransform_Identity, gd->atlas, NULL};
    sfVertex v[MAX_FLASHES * 6];
    size_t fresh[MAX_FLASHES];
    size_t n = flash_fresh(lv->seen_spent, st, fresh, MAX_FLASHES);

    for (size_t i = 0; i < n; i++)
        start_flash(lv, fresh[i]);
    n = 0;
    for (int i = 0; i < MAX_FLASHES; i++) {
        if (lv->flash_left[i] <= 0.0f)
            continue;
        object_flash_vertices(gd, &lv->sim.lvl.objects[lv->flash_index[i]],
            v + n, (sfUint8)(255.0f * lv->flash_left[i] / FLASH_TIME));
        n += 6;
        lv->flash_left[i] -= dt;
    }
    if (n > 0)
        draw_triangles(gd, v, n, &rs);
}

/* Three 110 x 110 frames of res/level/explos.png, over the death delay (6.1). */
static void render_explosion(gd_t *gd, level_t *lv)
{
    int done = DEATH_DELAY_TICKS - lv->death_ticks;
    int frame = done * EXPLOSION_FRAMES / DEATH_DELAY_TICKS;

    frame = frame < 0 ? 0 : (frame >= EXPLOSION_FRAMES ? EXPLOSION_FRAMES - 1
        : frame);
    sfSprite_setTextureRect(lv->explosion_sprite,
        (sfIntRect){frame * 110, 0, 110, 110});
    sfSprite_setPosition(lv->explosion_sprite,
        (sfVector2f){(float)lv->death_pos.x, (float)lv->death_pos.y});
    draw_sprite(gd, lv->explosion_sprite, NULL);
}

/*
** One tiled strip, 1200 px deep so the ground never ends inside the view
** whatever the camera does in a corridor (9.3). One draw call, and it works
** with a tile of any size: the texture is repeated, not stretched.
*/
static void render_ground(gd_t *gd, level_t *lv, float cam_x)
{
    sfVector2u ts = sfTexture_getSize(gd->res->ground);
    float x = floorf(cam_x / (float)ts.x) * (float)ts.x;

    sfSprite_setTextureRect(lv->ground_sprite,
        (sfIntRect){0, 0, (int)VIEW_W + 2 * (int)ts.x, 1200});
    sfSprite_setPosition(lv->ground_sprite, (sfVector2f){x, (float)GROUND_Y});
    draw_sprite(gd, lv->ground_sprite, NULL);
}

/*
** The background scrolls at a tenth of the camera, in the UI view, so it
** stays put while the world moves past it (9.3). One draw call.
*/
static void render_background(gd_t *gd, level_t *lv, vec2_t cam)
{
    sfSprite_setTextureRect(lv->background_sprite,
        (sfIntRect){(int)(cam.x * 0.1), (int)(cam.y * 0.1),
            (int)VIEW_W, (int)VIEW_H});
    sfSprite_setPosition(lv->background_sprite, (sfVector2f){0.0f, 0.0f});
    sfSprite_setScale(lv->background_sprite, (sfVector2f){1.0f, 1.0f});
    draw_sprite(gd, lv->background_sprite, NULL);
}

/*
** The wave's square is 30 px, too small to read: its icon is drawn at 60
** (8.4), and a mini one at half that, so mini still reads as smaller (10.4).
*/
#define ICON_MIN_SIDE 60.0

/* Texture, origin and scale for the current mode: both draw paths need them. */
static void setup_player_sprite(gd_t *gd, level_t *lv)
{
    const player_t *p = &lv->sim.st.player;
    sfTexture *const icons[MODE_COUNT] = {   /* one per mode (FEATURES 6.1) */
        [MODE_CUBE] = gd->res->player_icon,
        [MODE_SHIP] = gd->res->ship_icon,
        [MODE_UFO] = gd->res->ship_icon,     /* stands in until it has its own */
        [MODE_WAVE] = gd->res->player_icon,  /* the same */
        [MODE_BALL] = gd->res->player_icon,  /* the same, rolling */
        [MODE_ROBOT] = gd->res->player_icon, /* the same, upright */
        [MODE_SWING] = gd->res->ship_icon,   /* the ship's, for now */
        [MODE_SPIDER] = gd->res->player_icon, /* the cube's, upright */
    };
    sfTexture *tex = icons[p->mode];
    sfVector2u size = sfTexture_getSize(tex);
    double side = fmax(2.0 * player_mode(p)->half,
        ICON_MIN_SIDE * (p->mini ? MINI_SCALE : 1.0));

    sfSprite_setTexture(lv->player_sprite, tex, sfTrue);
    sfSprite_setOrigin(lv->player_sprite,
        (sfVector2f){size.x / 2.0f, size.y / 2.0f});
    sfSprite_setScale(lv->player_sprite,     /* upside down with its gravity */
        (sfVector2f){(float)(side / size.x),
        (float)(side / size.y) * (float)p->gravity_dir});
}

static void render_player(gd_t *gd, level_t *lv)
{
    const player_t *p = &lv->sim.st.player;
    vec2_t pos = drawn_player_pos(lv);

    render_trail(gd, lv, lv->state == LEVEL_DYING ? lv->death_pos : pos,
        (float)level_camera(gd, lv).x);
    if (lv->state == LEVEL_DYING)
        return;                              /* the explosion is its place */
    setup_player_sprite(gd, lv);
    sfSprite_setRotation(lv->player_sprite, p->rotation);
    sfSprite_setPosition(lv->player_sprite,
        (sfVector2f){(float)pos.x, (float)pos.y});
    draw_sprite(gd, lv->player_sprite, NULL);
}

/* The percentage and its bar, each if the settings show it (FEATURES 2.2). */
static void render_hud(gd_t *gd, level_t *lv)
{
    float pct = sim_percent(&lv->sim);
    char text[16];

    snprintf(text, sizeof(text), "%.0f%%", pct);
    if (strcmp(text, lv->shown_percent) != 0) {
        snprintf(lv->shown_percent, sizeof(lv->shown_percent), "%s", text);
        sfText_setString(lv->hud_text, text);
    }
    sfRectangleShape_setSize(lv->bar_fill,
        (sfVector2f){BAR_W * pct / 100.0f, BAR_H});
    sfRectangleShape_setFillColor(lv->bar_fill, lv->practice
        ? (sfColor){90, 190, 255, 255} : (sfColor){80, 220, 120, 255});
    if (gd->settings.show_progress_bar) {
        draw_rect(gd, lv->bar_back);
        draw_rect(gd, lv->bar_fill);
        practice_draw_bar(gd, lv, (sfVector2f){(VIEW_W - BAR_W) / 2.0f,
            30.0f}, (sfVector2f){BAR_W, BAR_H});
    }
    sfText_setPosition(lv->hud_text,
        (sfVector2f){(VIEW_W + BAR_W) / 2.0f + 20.0f, 28.0f});
    if (gd->settings.show_percent)
        draw_text(gd, lv->hud_text);
}

/*
** GD draws "Attempt N" in the world near the spawn, so it scrolls away on its
** own with no timer (9.5).
*/
static void render_attempt(gd_t *gd, level_t *lv)
{
    if (!gd->settings.show_attempts)
        return;
    sfText_setPosition(lv->attempt_text,
        (sfVector2f){(float)level_current_start(lv)->pos.x + 400.0f, 400.0f});
    draw_text(gd, lv->attempt_text);
}

/*
** The camera freezes with the player PLAYER_SCREEN_X from the left, and the
** wall stands at the right edge of that frozen view: this is how far the
** player's front has to fly to touch it, whatever the level (9.8).
*/
#define END_DISTANCE (VIEW_W - END_WALL_W - PLAYER_SCREEN_X - PLAYER_HALF)

/* The run's own speed, so a level finished at 4x doesn't crawl at the end. */
static double end_speed(const level_t *lv)
{
    return lv->sim.st.player.vx * TICK_RATE;
}

float level_end_flight(const level_t *lv)
{
    return (float)(END_DISTANCE / end_speed(lv));
}

/* The wall the player flies into at the end: the frozen view's edge (9.8). */
static void draw_end_wall(gd_t *gd, level_t *lv, vec2_t cam)
{
    sfSprite_setTextureRect(lv->strip_sprite,
        (sfIntRect){0, 0, (int)END_WALL_W, (int)VIEW_H});
    sfSprite_setColor(lv->strip_sprite, sfWhite);
    sfSprite_setPosition(lv->strip_sprite,
        (sfVector2f){(float)cam.x + VIEW_W - END_WALL_W, (float)cam.y});
    draw_sprite(gd, lv->strip_sprite, NULL);
}

/*
** The finish is a moment, not a cut (9.8): the camera stops, the player flies
** on at its own speed until it touches the end wall, then a white flash, then
** the end screen.
*/
static void render_end_sequence(gd_t *gd, level_t *lv, vec2_t cam)
{
    const player_t *p = &lv->sim.st.player;
    float t = lv->end_time;
    float flight = level_end_flight(lv);
    double x = p->pos.x + (double)fminf(t, flight) * end_speed(lv);

    sfView_setCenter(gd->level_view, (sfVector2f){(float)cam.x + VIEW_W / 2.0f,
        (float)cam.y + VIEW_H / 2.0f});
    sfRenderWindow_setView(gd->w, gd->ui_view);
    render_background(gd, lv, cam);
    sfRenderWindow_setView(gd->w, gd->level_view);
    render_ground(gd, lv, (float)cam.x);
    render_ship_bounds(gd, lv, (float)cam.x);  /* a level may end in one */
    render_objects(gd, lv, (float)cam.x, false);
    draw_end_wall(gd, lv, cam);
    render_trail(gd, lv, (vec2_t){x, p->pos.y}, (float)cam.x);
    setup_player_sprite(gd, lv);
    sfSprite_setPosition(lv->player_sprite,
        (sfVector2f){(float)x, (float)p->pos.y});
    sfSprite_setRotation(lv->player_sprite,
        p->rotation + fminf(t, flight) * (float)CUBE_SPIN);
    draw_sprite(gd, lv->player_sprite, NULL);
    render_objects(gd, lv, (float)cam.x, true);
    sfRenderWindow_setView(gd->w, gd->ui_view);
    if (t < flight)
        return;
    sfRectangleShape_setFillColor(lv->flash, (sfColor){255, 255, 255,
        (sfUint8)(255.0f * fmaxf(0.0f,
            1.0f - (t - flight) / END_FLASH))});
    draw_rect(gd, lv->flash);
}

void level_render(gd_t *gd, level_t *lv)
{
    vec2_t cam = level_camera(gd, lv);

    gd->draw_calls = 0;                      /* counted by draw.c (9.0) */

    if (lv->state == LEVEL_COMPLETE) {
        lv->end_time += (float)lv->frame_us / 1000000.0f;
        if (lv->end_time < level_end_flight(lv) + END_FLASH)
            return render_end_sequence(gd, lv, cam);
        sfRenderWindow_setView(gd->w, gd->ui_view);
        print_end_level_screen(gd, lv->end_screen);
        print_cursor(gd->cursor, gd->w);
        return;
    }
    sfRenderWindow_setView(gd->w, gd->ui_view);
    render_background(gd, lv, cam);          /* screen space, parallax (9.3) */
    sfView_setCenter(gd->level_view, (sfVector2f){(float)cam.x + VIEW_W / 2.0f,
        (float)cam.y + VIEW_H / 2.0f});
    sfRenderWindow_setView(gd->w, gd->level_view);
    render_ground(gd, lv, (float)cam.x);
    render_ship_bounds(gd, lv, (float)cam.x);
    render_objects(gd, lv, (float)cam.x, false);   /* B5 to B1 (11.15) */
    render_player(gd, lv);
    render_objects(gd, lv, (float)cam.x, true);    /* T1 to T4 */
    if (lv->state == LEVEL_DYING)
        render_explosion(gd, lv);            /* over everything it hit */
    practice_draw_world(gd, lv, (float)cam.x);   /* its checkpoints (13.1) */
    render_spent_flashes(gd, lv);
    render_attempt(gd, lv);
    if (gd->debug_overlay)
        render_debug_overlay(gd, lv, cam);
    sfRenderWindow_setView(gd->w, gd->ui_view);
    render_hud(gd, lv);
    if (lv->notice_left > 0.0f)
        draw_text(gd, lv->notice_text);      /* "Song missing" (FEATURES 4.4) */
}
