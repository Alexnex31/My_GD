/*
** ALEXNEX PROJECT, 2026
** level_render
** File description:
** drawing a level: views, camera, objects, the player (9.1, 9.4)
*/

#include "mygd.h"
#include "sim/modes.h"

#define DRAW_MARGIN 200.0f        /* a sprite can reach past its hitbox (9.2) */

/* A whole screen pixel, so tiles don't shimmer on a fractional camera (9.1). */
static float snap(gd_t *gd, double v)
{
    float scale = gd->viewport_px_w / VIEW_W;

    if (scale <= 0.0f)
        return (float)v;
    return roundf((float)v * scale) / scale;
}

vec2_t level_camera(gd_t *gd, const level_t *lv)
{
    const run_state_t *st = &lv->sim.st;

    return (vec2_t){snap(gd, st->player.pos.x - PLAYER_SCREEN_X),
        snap(gd, st->cam.pos.y)};
}

static sfTexture *texture_for(gd_t *gd, const object_t *o)
{
    if (o->type == OBJ_SPIKE)
        return gd->res->spike;
    if (o->type == OBJ_PORTAL)
        return o->portal_mode == MODE_SHIP ? gd->res->ship_icon
            : gd->res->player_icon;
    return gd->res->block;
}

/* One sprite reused for every object, scaled to the object's own rect. */
static void draw_object(gd_t *gd, level_t *lv, const object_t *o)
{
    sfSprite *s = lv->object_sprite;
    sfTexture *tex = texture_for(gd, o);
    sfVector2u size = sfTexture_getSize(tex);

    sfSprite_setTexture(s, tex, sfTrue);
    sfSprite_setOrigin(s, (sfVector2f){size.x / 2.0f, size.y / 2.0f});
    sfSprite_setScale(s, (sfVector2f){(float)o->rect.w / (float)size.x,
        (float)o->rect.h / (float)size.y});
    sfSprite_setRotation(s, (float)o->rotation);
    sfSprite_setPosition(s, (sfVector2f){(float)(o->rect.x + o->rect.w / 2.0),
        (float)(o->rect.y + o->rect.h / 2.0)});
    sfSprite_setColor(s, o->type == OBJ_PORTAL && is_spent(&lv->sim.st,
        (size_t)(o - lv->sim.lvl.objects))
        ? (sfColor){255, 255, 255, 140} : sfWhite);
    sfRenderWindow_drawSprite(gd->w, s, NULL);
}

/*
** The objects are sorted by x (4.1), so the walk stops at the first one past
** the view. The margin covers a sprite reaching past its hitbox: a spike's
** hitbox is inset inside its 100x100 sprite (4.2).
*/
static void render_objects(gd_t *gd, level_t *lv, float cam_x)
{
    const level_data_t *lvl = &lv->sim.lvl;

    for (size_t i = 0; i < lvl->nb_objects; i++) {
        const object_t *o = &lvl->objects[i];

        if (o->hitbox.aabb.x > cam_x + VIEW_W + DRAW_MARGIN)
            break;
        if (o->hitbox.aabb.x + o->hitbox.aabb.w < cam_x - DRAW_MARGIN)
            continue;
        draw_object(gd, lv, o);
    }
}

static void render_ground(gd_t *gd, level_t *lv, float cam_x)
{
    sfVector2u size = sfTexture_getSize(gd->res->ground);
    float x = floorf(cam_x / (float)size.x) * (float)size.x;

    for (int i = 0; i < 3; i++) {
        sfSprite_setPosition(lv->ground_sprite,
            (sfVector2f){x + i * (float)size.x, (float)GROUND_Y});
        sfRenderWindow_drawSprite(gd->w, lv->ground_sprite, NULL);
    }
}

static void render_player(gd_t *gd, level_t *lv)
{
    const player_t *p = &lv->sim.st.player;
    sfTexture *tex = p->mode == MODE_SHIP ? gd->res->ship_icon
        : gd->res->player_icon;
    sfVector2u size = sfTexture_getSize(tex);
    double side = 2.0 * MODES[p->mode].half;

    if (lv->state == LEVEL_DYING)
        return;                              /* the explosion takes its place */
    sfSprite_setTexture(lv->player_sprite, tex, sfTrue);
    sfSprite_setOrigin(lv->player_sprite,
        (sfVector2f){size.x / 2.0f, size.y / 2.0f});
    sfSprite_setScale(lv->player_sprite,
        (sfVector2f){(float)(side / size.x), (float)(side / size.y)});
    sfSprite_setRotation(lv->player_sprite, p->rotation);
    sfSprite_setPosition(lv->player_sprite,
        (sfVector2f){(float)p->pos.x, (float)p->pos.y});
    sfRenderWindow_drawSprite(gd->w, lv->player_sprite, NULL);
}

static void render_hud(gd_t *gd, level_t *lv)
{
    char text[128];

    snprintf(text, sizeof(text), "%.2f%%   Attempt %d",
        sim_percent(&lv->sim), lv->stats.attempts);
    sfText_setString(lv->hud_text, text);
    sfText_setPosition(lv->hud_text, (sfVector2f){40.0f, 30.0f});
    sfRenderWindow_drawText(gd->w, lv->hud_text, NULL);
}

void level_render(gd_t *gd, level_t *lv)
{
    vec2_t cam = level_camera(gd, lv);

    if (lv->state == LEVEL_COMPLETE && lv->end_screen != NULL) {
        sfRenderWindow_setView(gd->w, gd->ui_view);
        print_end_level_screen(gd, lv->end_screen);
        print_cursor(gd->cursor, gd->w);
        return;
    }
    sfView_setCenter(gd->level_view, (sfVector2f){(float)cam.x + VIEW_W / 2.0f,
        (float)cam.y + VIEW_H / 2.0f});
    sfRenderWindow_setView(gd->w, gd->level_view);
    render_ground(gd, lv, (float)cam.x);
    render_objects(gd, lv, (float)cam.x);
    render_player(gd, lv);
    if (gd->debug_overlay)
        render_debug_overlay(gd, lv, cam);
    sfRenderWindow_setView(gd->w, gd->ui_view);
    render_hud(gd, lv);
}
