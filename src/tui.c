#include "tui.h"
#include "utilities.h"

#include <notcurses/notcurses.h>
#include <cjson/cJSON.h>
#include <unistd.h>

typedef struct {
    struct ncplane *parent;
    struct ncplane *current;
    struct ncplane *preview;
} panes_t;

static struct ncplane *make_pane(struct ncplane *std, int y, int x, unsigned rows, unsigned cols, uint32_t border_rgb) {
    struct ncplane_options opts = {
        .y = y,
        .x = x,
        .rows = rows,
        .cols = cols,
    };
    struct ncplane *p = ncplane_create(std, &opts);
    if (!p) {
        return NULL;
    }

    uint64_t channels = 0;
    ncchannels_set_fg_rgb(&channels, border_rgb);
    ncplane_perimeter_rounded(p, 0, channels, 0);

    return p;
}

static void destroy_panes(panes_t *panes) {
    ncplane_destroy(panes->parent);
    ncplane_destroy(panes->current);
    ncplane_destroy(panes->preview);
    panes->parent = NULL;
    panes->current = NULL;
    panes->preview = NULL;
}

static int layout_panes(struct notcurses *nc, panes_t *panes) {
    struct ncplane *std = notcurses_stdplane(nc);
    unsigned rows, cols;
    ncplane_dim_yx(std, &rows, &cols);

    unsigned unit = cols / 8; // 1 + 4 + 3 = 8 parts
    unsigned w_parent = unit;
    unsigned w_current = unit * 4;
    // give preview whatever's left so rounding doesn't leave a gap
    unsigned w_preview = cols - w_parent - w_current;

    // Dracula-ish palette: dim purple-grey borders, current pane brighter
    panes->parent  = make_pane(std, 0, 0,                           rows, w_parent,  0x6272a4);
    panes->current = make_pane(std, 0, (int)w_parent,               rows, w_current, 0xbd93f9);
    panes->preview = make_pane(std, 0, (int)(w_parent + w_current), rows, w_preview, 0x6272a4);

    if (!panes->parent || !panes->current || !panes->preview) {
        destroy_panes(panes);
        return -1;
    }

    ncplane_putstr_yx(panes->parent,  1, 2, "parent");
    ncplane_putstr_yx(panes->current, 1, 2, "current");
    ncplane_putstr_yx(panes->preview, 1, 2, "preview");
    ncplane_putstr_yx(panes->preview, 3, 2, "Press 'q' or ESC to exit.");

    return 0;
}

int sigma360_tui(void) {
    cJSON *json = get_json("./src/cmds/courses.json");
    if (!json) {
        fprintf(stderr, "[ERROR] Failed to load JSON data.\n");
        return 1;
    }
    char* str = cJSON_Print(json);
    if (!str) {
        fprintf(stderr, "[ERROR] Failed to print JSON data.\n");
        cJSON_Delete(json);
        return 1;
    }
    printf("%s\n", str);
    free(str);
    cJSON_Delete(json);

    struct notcurses_options opts = {0};
    opts.flags = NCOPTION_SUPPRESS_BANNERS;

    struct notcurses *nc = notcurses_core_init(&opts, NULL);
    if (!nc) {
        return 1;
    }

    panes_t panes = {0};
    if (layout_panes(nc, &panes) != 0) {
        notcurses_stop(nc);
        return 1;
    }
    notcurses_render(nc);

    struct ncinput ni;
    uint32_t id;
    for (;;) {
        id = notcurses_get_blocking(nc, &ni);

        if (id == (uint32_t)-1) {
            break; // error
        }
        if (ni.evtype == NCTYPE_RELEASE) {
            continue; // ignore key-up on Kitty-protocol terminals
        }
        if (id == NCKEY_RESIZE) {
            destroy_panes(&panes);
            if (layout_panes(nc, &panes) != 0) {
                break;
            }
            notcurses_render(nc);
            continue;
        }
        if (id == 'q' || id == NCKEY_ESC) {
            break;
        }
    }

    destroy_panes(&panes);
    notcurses_stop(nc);
    return 0;
}