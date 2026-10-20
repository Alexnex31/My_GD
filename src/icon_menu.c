/*
** ALEXNEX PROJECT, 2026
** icon_menu
** File description:
** the Icons screen: an icon per gamemode, chosen among its folder's (FEATURES 3.8)
*/

#include "mygd.h"

    #define TAB_SIDE 130.0f
    #define TAB_STEP 160.0f
    #define TAB_Y 150.0f
    #define CELL_SIDE 110.0f
    #define CELL_STEP 135.0f
    #define GRID_COLS 10
    #define GRID_Y 400.0f
    #define ICON_FILL 0.75f           /* of its box: the frame shows around it */

static const sfColor BOX = {20, 24, 40, 170};
static const sfColor SHOWN = {40, 120, 70, 230};
static const sfColor CHOSEN = {90, 255, 140, 255};

static const char *const TITLES[MODE_COUNT] = {[MODE_CUBE] = "Cube",
    [MODE_SHIP] = "Ship", [MODE_UFO] = "UFO", [MODE_WAVE] = "Wave",
    [MODE_BALL] = "Ball", [MODE_ROBOT] = "Robot", [MODE_SWING] = "Swing",
    [MODE_SPIDER] = "Spider"};

/* The page's cells hold the tab's icons; the others are off. */
static void show_page(icon_m_t *im)
{
    int count = im->gd->icons.count[im->tab];
    bool pages = icon_kit_pages(&im->gd->icons, im->tab) > 1;

    for (int k = 0; k < ICONS_PER_PAGE; k++)
        im->widgets[ICON_W_GRID + k].enabled = im->page * ICONS_PER_PAGE + k
            < count;
    im->widgets[ICON_W_PREV].enabled = pages;
    im->widgets[ICON_W_NEXT].enabled = pages;
}

/* A tab shows its mode, on the page of the icon it has. */
static void tab_chosen(void *ctx, widget_t *w)
{
    icon_m_t *im = ctx;

    im->tab = ICON_KIT_ORDER[w - &im->widgets[ICON_W_TABS]];
    im->page = icon_kit_chosen(&im->gd->icons, &im->gd->settings, im->tab)
        / ICONS_PER_PAGE;
    show_page(im);
}

static void cell_chosen(void *ctx, widget_t *w)
{
    icon_m_t *im = ctx;

    icon_kit_choose(&im->gd->icons, &im->gd->settings, im->tab,
        im->page * ICONS_PER_PAGE + (int)(w - &im->widgets[ICON_W_GRID]));
}

static void page_turned(void *ctx, widget_t *w)
{
    icon_m_t *im = ctx;
    int pages = icon_kit_pages(&im->gd->icons, im->tab);

    im->page = (im->page + (w == &im->widgets[ICON_W_PREV] ? pages - 1 : 1))
        % pages;
    show_page(im);
}

static void icon_back(void *ctx)
{
    ((icon_m_t *)ctx)->back = true;
}

static void back_pressed(void *ctx, widget_t *w)
{
    (void)w;
    icon_back(ctx);
}

static void lay_out(icon_m_t *im)
{
    float tabs_x = (VIEW_W - (MODE_COUNT * TAB_STEP - (TAB_STEP - TAB_SIDE)))
        / 2.0f;
    float grid_x = (VIEW_W - (GRID_COLS * CELL_STEP - (CELL_STEP
        - CELL_SIDE))) / 2.0f;

    for (int t = 0; t < MODE_COUNT; t++)
        im->widgets[ICON_W_TABS + t] = (widget_t){.kind = W_BUTTON,
            .label = TITLES[ICON_KIT_ORDER[t]], .enabled = true,
            .bounds = {tabs_x + TAB_STEP * (float)t, TAB_Y, TAB_SIDE,
            TAB_SIDE}, .on_activate = tab_chosen};
    for (int k = 0; k < ICONS_PER_PAGE; k++)
        im->widgets[ICON_W_GRID + k] = (widget_t){.kind = W_BUTTON,
            .label = "", .bounds = {grid_x + CELL_STEP * (float)(k
            % GRID_COLS), GRID_Y + CELL_STEP * (float)(k / GRID_COLS),
            CELL_SIDE, CELL_SIDE}, .on_activate = cell_chosen};
    im->widgets[ICON_W_PREV] = (widget_t){.kind = W_BUTTON, .label = "<",
        .bounds = {grid_x - 140.0f, GRID_Y + CELL_STEP, 90.0f, CELL_SIDE},
        .on_activate = page_turned};
    im->widgets[ICON_W_NEXT] = (widget_t){.kind = W_BUTTON, .label = ">",
        .bounds = {VIEW_W - grid_x + 50.0f, GRID_Y + CELL_STEP, 90.0f,
        CELL_SIDE}, .on_activate = page_turned};
    im->widgets[ICON_W_BACK] = (widget_t){.kind = W_BUTTON, .label = "Back",
        .enabled = true, .bounds = {810.0f, 950.0f, 300.0f, 80.0f},
        .on_activate = back_pressed};
}

/* The cube's tab first, as GD's kit opens; the arrows go by position. */
icon_m_t *create_icon_menu(gd_t *gd)
{
    icon_m_t *im = sim_xcalloc(1, sizeof(icon_m_t));

    im->gd = gd;
    im->background = sfSprite_create();
    im->icon = sfSprite_create();
    sfSprite_setTexture(im->background, gd->res->list_background, sfTrue);
    lay_out(im);
    ui_init(&im->ui, im->widgets, ICON_W_COUNT, im);
    im->ui.grid = true;
    im->ui.on_back = icon_back;
    tab_chosen(im, &im->widgets[ICON_W_TABS]);
    gd->menu = 'i';
    return im;
}

void free_icon_menu(icon_m_t *im)
{
    if (im == NULL)
        return;
    sfSprite_destroy(im->background);
    sfSprite_destroy(im->icon);
    free(im);
}

/* The image fitted into the widget, its shape kept, with the widget's looks. */
static void draw_icon(icon_m_t *im, int i, sfTexture *tex)
{
    sfVector2u size = sfTexture_getSize(tex);
    float k = im->widgets[i].bounds.w * ICON_FILL
        / (float)(size.x > size.y ? size.x : size.y);

    sfSprite_setTexture(im->icon, tex, sfTrue);
    sfSprite_setScale(im->icon, (sfVector2f){k, k});
    ui_draw_sprite(im->gd, &im->ui, i, im->icon);
}

static void draw_tabs(icon_m_t *im)
{
    for (int t = 0; t < MODE_COUNT; t++) {
        gamemode_t mode = ICON_KIT_ORDER[t];

        ui_box(im->gd, im->widgets[ICON_W_TABS + t].bounds,
            mode == im->tab ? SHOWN : BOX, mode == im->tab ? CHOSEN
            : sfTransparent);
        draw_icon(im, ICON_W_TABS + t, icon_texture(im->gd, mode));
    }
}

static void draw_grid(icon_m_t *im)
{
    const icon_kit_t *kit = &im->gd->icons;
    int chosen = icon_kit_chosen(kit, &im->gd->settings, im->tab);

    for (int k = 0; k < ICONS_PER_PAGE; k++) {
        int i = im->page * ICONS_PER_PAGE + k;

        if (i >= kit->count[im->tab])
            break;
        ui_box(im->gd, im->widgets[ICON_W_GRID + k].bounds, BOX,
            i == chosen ? CHOSEN : sfTransparent);
        draw_icon(im, ICON_W_GRID + k, im->gd->icon_tex[im->tab][i]);
    }
}

void print_icon_menu(icon_m_t *im, gd_t *gd)
{
    char line[64];
    int pages = icon_kit_pages(&gd->icons, im->tab);

    sfRenderWindow_drawSprite(gd->w, im->background, NULL);
    ui_update(&im->ui, ui_now_ms());
    ui_caption(gd, "Icons", VIEW_W / 2.0f, 80.0f);
    draw_tabs(im);
    snprintf(line, sizeof(line), "%s", TITLES[im->tab]);
    if (pages > 1)
        snprintf(line, sizeof(line), "%s   %d / %d", TITLES[im->tab],
            im->page + 1, pages);
    ui_caption(gd, line, VIEW_W / 2.0f, 345.0f);
    draw_grid(im);
    if (pages > 1) {
        ui_draw_widget(gd, &im->ui, ICON_W_PREV);
        ui_draw_widget(gd, &im->ui, ICON_W_NEXT);
    }
    ui_draw_widget(gd, &im->ui, ICON_W_BACK);
}

/* Leaving saves the choice if it changed (2.4); the main menu takes over. */
void keyboard_events_icon_menu(icon_m_t **im, gd_t *gd)
{
    ui_event_t ev;

    while (poll_event(gd)) {
        if (gd->event->type == sfEvtClosed)
            return close_window(gd->w);
        if (ui_from_sf(gd, gd->event, &ev))
            ui_event(&(*im)->ui, &ev, ui_now_ms());
    }
    if (!(*im)->back)
        return;
    if (gd->settings.dirty && !gd->settings.unreadable
        && settings_save(&gd->settings) != 0)
        dprintf(2, "my_gd: cannot save %s\n", gd->settings.path);
    free_icon_menu(*im);
    *im = NULL;
    gd->menu = 'm';
}
