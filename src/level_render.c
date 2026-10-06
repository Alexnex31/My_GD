/*
** ALEXNEX PROJECT, 2026
** level_render
** File description:
** drawing a level: views, camera, objects, the player (9.1, 9.4)
*/

#include "mygd.h"
#include "sim/modes.h"

#define DRAW_MARGIN 200.0f        /* a sprite can reach past its hitbox (9.2) */
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

/* One object drawn on its own, from the atlas: the spent flash (9.2). */
static void draw_one_object(gd_t *gd, level_t *lv, const object_t *o,
    sfColor col)
{
    sfFloatRect a = gd->atlas_rect[o->type];

    sfSprite_setTextureRect(lv->flash_sprite, (sfIntRect){(int)a.left,
        (int)a.top, (int)a.width, (int)a.height});
    sfSprite_setOrigin(lv->flash_sprite, (sfVector2f){a.width / 2.0f,
        a.height / 2.0f});
    sfSprite_setScale(lv->flash_sprite, (sfVector2f){(float)o->rect.w / a.width,
        (float)o->rect.h / a.height});
    sfSprite_setRotation(lv->flash_sprite, (float)o->rotation);
    sfSprite_setPosition(lv->flash_sprite,
        (sfVector2f){(float)(o->rect.x + o->rect.w / 2.0),
            (float)(o->rect.y + o->rect.h / 2.0)});
    sfSprite_setColor(lv->flash_sprite, col);
    draw_sprite(gd, lv->flash_sprite, NULL);
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
** one white frame fading out over it. The spent bitset is compared word by
** word with the one the last frame drew, so this costs nothing per object.
*/
static void render_spent_flashes(gd_t *gd, level_t *lv)
{
    const run_state_t *st = &lv->sim.st;
    float dt = (float)lv->frame_us / 1000000.0f;

    for (size_t w = 0; w < st->spent_words; w++) {
        uint64_t fresh = st->spent[w] & ~lv->seen_spent[w];

        for (int b = 0; fresh != 0 && b < 64; b++)
            if (fresh & ((uint64_t)1 << b)) {
                start_flash(lv, w * 64 + b);
                fresh &= ~((uint64_t)1 << b);
            }
        lv->seen_spent[w] = st->spent[w];
    }
    for (int i = 0; i < MAX_FLASHES; i++) {
        if (lv->flash_left[i] <= 0.0f)
            continue;
        draw_one_object(gd, lv, &lv->sim.lvl.objects[lv->flash_index[i]],
            (sfColor){255, 255, 255,
                (sfUint8)(255.0f * lv->flash_left[i] / FLASH_TIME)});
        lv->flash_left[i] -= dt;
    }
}

/* Three 110 x 110 frames of res/explos.png, over the death delay (6.1). */
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
    sfVector2u ts = sfTexture_getSize(gd->res->level_background);

    sfSprite_setTextureRect(lv->background_sprite,
        (sfIntRect){(int)(cam.x * 0.1), (int)(cam.y * 0.1),
            (int)VIEW_W, (int)VIEW_H});
    sfSprite_setPosition(lv->background_sprite, (sfVector2f){0.0f, 0.0f});
    sfSprite_setScale(lv->background_sprite, (sfVector2f){1.0f, 1.0f});
    draw_sprite(gd, lv->background_sprite, NULL);
    (void)ts;
}

/* The wave's square is 30 px, too small to read: its icon is drawn at 60 (8.4). */
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
    };
    sfTexture *tex = icons[p->mode];
    sfVector2u size = sfTexture_getSize(tex);
    double side = fmax(2.0 * MODES[p->mode].half, ICON_MIN_SIDE);

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
        return render_explosion(gd, lv);     /* it takes the player's place */
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
    if (gd->settings.show_progress_bar) {
        draw_rect(gd, lv->bar_back);
        draw_rect(gd, lv->bar_fill);
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
        (sfVector2f){(float)lv->sim.lvl.hdr.start.pos.x + 400.0f, 400.0f});
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
    render_objects(gd, lv, (float)cam.x);
    draw_end_wall(gd, lv, cam);
    render_trail(gd, lv, (vec2_t){x, p->pos.y}, (float)cam.x);
    setup_player_sprite(gd, lv);
    sfSprite_setPosition(lv->player_sprite,
        (sfVector2f){(float)x, (float)p->pos.y});
    sfSprite_setRotation(lv->player_sprite,
        p->rotation + fminf(t, flight) * (float)CUBE_SPIN);
    draw_sprite(gd, lv->player_sprite, NULL);
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
    render_objects(gd, lv, (float)cam.x);
    render_spent_flashes(gd, lv);
    render_player(gd, lv);
    render_attempt(gd, lv);
    if (gd->debug_overlay)
        render_debug_overlay(gd, lv, cam);
    sfRenderWindow_setView(gd->w, gd->ui_view);
    render_hud(gd, lv);
    if (lv->notice_left > 0.0f)
        draw_text(gd, lv->notice_text);      /* "Song missing" (FEATURES 4.4) */
}
