#include "tui.h"
#include "nav.h"
#include "utilities.h"

#include <notcurses/nckeys.h>
#include <notcurses/notcurses.h>
#include <cjson/cJSON.h>

#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/wait.h>

#define COL_BORDER_DIM    0x6272a4u
#define COL_BORDER_ACTIVE 0xbd93f9u
#define COL_SEL_BG_ACTIVE 0x2f7482u
#define COL_SEL_BG_IDLE   0x44475au
#define COL_SEL_FG        0xf8f8f2u

#define DETAILS_MIN_COLS 10

typedef struct {
    struct ncplane *frame;
    struct ncplane *content;
} pane_t;

typedef struct {
    pane_t parent;
    pane_t current;
    pane_t preview;
} panes_t;

typedef enum {
    ROLE_PARENT,
    ROLE_CURRENT,
    ROLE_PREVIEW,
} panerole_t;

// ------------------------------------------- //
// Panes                                       //
// ------------------------------------------- //

static void destroy_pane(pane_t *pane) {
    if (pane->content != NULL) {
        ncplane_destroy(pane->content);
    }
    if (pane->frame != NULL) {
        ncplane_destroy(pane->frame);
    }
    pane->frame = NULL;
    pane->content = NULL;
}

static void destroy_panes(panes_t *panes) {
    destroy_pane(&panes->parent);
    destroy_pane(&panes->current);
    destroy_pane(&panes->preview);
}

static int make_pane(struct ncplane *std, pane_t *pane, int y, int x, unsigned rows, unsigned cols, uint32_t border_rgb) {
    pane->frame = NULL;
    pane->content = NULL;

    if (rows < 3 || cols < 3) {
        return -1; // not enough space for a pane
    }
    
    struct ncplane_options fopts = {
        .y = y,
        .x = x,
        .rows = rows,
        .cols = cols,
    };
    pane->frame = ncplane_create(std, &fopts);
    if (pane->frame == NULL) {
        return -1;
    }

    uint64_t channels = 0;
    ncchannels_set_fg_rgb(&channels, border_rgb);
    ncplane_perimeter_rounded(pane->frame, 0, channels, 0);

    struct ncplane_options copts = {
        .y = 1,
        .x = 1,
        .rows = rows - 2,
        .cols = cols - 2,
    };
    pane->content = ncplane_create(pane->frame, &copts);
    if (pane->content == NULL) {
        destroy_pane(pane);
        return -1;
    }

    return 0;
}

static int layout_panes(struct notcurses *nc, panes_t *panes) {
    struct ncplane *std = notcurses_stdplane(nc);
    unsigned rows, cols;
    ncplane_dim_yx(std, &rows, &cols);

    unsigned unit = cols / 8;
    unsigned parentw = unit;
    unsigned currentw = unit * 4;
    unsigned previeww = cols - parentw - currentw;

    if (make_pane(std, &panes->parent,  0, 0, rows, cols,  COL_BORDER_DIM) != 0 ||
        make_pane(std, &panes->current, 0, (int)parentw, rows, currentw, COL_BORDER_ACTIVE) != 0 ||
        make_pane(std, &panes->preview, 0, (int)(parentw + currentw), rows, previeww, COL_BORDER_DIM) != 0) {
        destroy_panes(panes);
        return -1;
    }

    return 0;
}

// ------------------------------------------- //
// Rendering                                   //
// ------------------------------------------- //

static void clamp_view(list_t *l, unsigned rows) {
    if (rows == 0) {
        return;
    }
    if (l->count <= rows) {
        l->top = 0;
        return;
    }
    if (l->sel < l->top) {
        l->top = l->sel;
    } else if (l->sel >= l->top + rows) {
        l->top = l->sel - rows + 1;
    }
    if (l->top > l->count - rows) {
        l->top = l->count - rows;
    }
}

static void draw_list(struct ncplane *p, list_t *l, panerole_t role) {
    if (p == NULL) {
        return;
    }
    ncplane_erase(p);
    if (l == NULL || l->count == 0) {
        return;
    }

    unsigned rows = ncplane_dim_y(p);
    unsigned cols = ncplane_dim_x(p);
    if (rows == 0 || cols == 0) {
        return;
    }
    clamp_view(l, rows);

    int w = (int)cols - 1;
    if (w < 1) {
        w = 1;
    }

    for (size_t i = l->top; i < l->count && (i - l->top) < rows; i++) {
        const entry_t *e = &l->items[i];

        char line[512];
        if (role == ROLE_CURRENT && cols > DETAILS_MIN_COLS && e->detail != NULL) {
            snprintf(line, sizeof line, "%s - %s", e->label, e->detail);
        } else {
            snprintf(line, sizeof line, "%s", (e->label != NULL) ? e->label : "");
        }
 
        if (i == l->sel) {
            ncplane_set_bg_rgb(p, (role == ROLE_CURRENT) ? COL_SEL_BG_ACTIVE : COL_SEL_BG_IDLE);
            ncplane_set_fg_rgb(p, COL_SEL_FG);
        } else {
            ncplane_set_bg_default(p);
            ncplane_set_fg_default(p);
        }
 
        // '%-*.*s' pads to the full pane width so the selected row reads as a solid bar, and truncates anything longer.
        ncplane_printf_yx(p, (int)(i - l->top), 0, " %-*.*s", w, w, line);
    }

    ncplane_set_bg_default(p);
    ncplane_set_fg_default(p);
}

static void draw_all(panes_t *panes, nav_t *nav) {
    draw_list(panes->parent.content,  nav_parent(nav),  ROLE_PARENT);
    draw_list(panes->current.content, nav_current(nav), ROLE_CURRENT);
    draw_list(panes->preview.content, nav_preview(nav), ROLE_PREVIEW);
}

// ------------------------------------------- //
// Main                                        //
// ------------------------------------------- //

static int sigma360_tui_watch(const char *course, int lecture);

int sigma360_tui(void) {
    cJSON *json = get_json("./src/cmds/courses.json");
    if (!json) {
        fprintf(stderr, "[ERROR] Failed to load JSON data.\n");
        return 1;
    }
    sort_cjson_array(json);

    nav_t nav;
    if (nav_init(&nav, json) != 0) {
        fprintf(stderr, "[ERROR] Failed to build navigation tree.\n");
        cJSON_Delete(json);
        return 1;
    }

    struct notcurses_options opts = {0};
    opts.flags = NCOPTION_SUPPRESS_BANNERS;

    struct notcurses *nc = notcurses_core_init(&opts, NULL);
    if (nc == NULL) {
        nav_free(&nav);
        cJSON_Delete(json);
        return 1;
    }

    panes_t panes = {0};
    if (layout_panes(nc, &panes) != 0) {
        notcurses_stop(nc);
        nav_free(&nav);
        cJSON_Delete(json);
        return 1;
    }

    draw_all(&panes, &nav);
    notcurses_render(nc);

    struct ncinput ni;
    for (;;) {
        uint32_t id = notcurses_get_blocking(nc, &ni);

        if (id == (uint32_t)-1) {
            break; // error
        }
        if (ni.evtype == NCTYPE_RELEASE) {
            continue; // ignore key-up on Kitty-protocol terminals
        }
        if (id == 'q' || id == NCKEY_ESC) {
            break; // quiting out
        }

        if (id == NCKEY_RESIZE) {
            destroy_panes(&panes);
            if (layout_panes(nc, &panes) != 0) {
                break; // terminal too small probably
            }
            break;
        }

        else if (id == 'j' || id == NCKEY_DOWN) {
            nav_move(&nav, 1);
        } else if (id == 'k' || id == NCKEY_UP) {
            nav_move(&nav, -1);
        } else if (id == 'l' || id == NCKEY_RIGHT) {
            nav_descend(&nav);
        } else if (id == 'h' || id == NCKEY_LEFT) {
            nav_ascend(&nav);
        } else if (id == NCKEY_ENTER) {
            if (!nav_descend(&nav)) {
                list_t *l = nav_current(&nav);
                if (nav.depth > 0 && l->count > 0) {
                    sigma360_tui_watch(nav.path[nav.depth]->label, (int)l->sel + 1);
                }
            }
        } else {
            continue; // some unbound key; no redraw required
        }

        draw_all(&panes, &nav);
        notcurses_render(nc);
    }

    destroy_panes(&panes);
    notcurses_stop(nc);
    nav_free(&nav);
    cJSON_Delete(json);
    return 0;
}

static int sigma360_tui_watch(const char *course, int lecture) {
    char path[256];
    int n = snprintf(path, sizeof(path), "/%s/%d", (course != NULL) ? course : "unknown", lecture);
    if (n < 0 || (size_t)n >= sizeof(path)) {
        return -1;
    }

    pid_t pid = fork();
    if (pid < 0) {
        return -1;
    }

    if (pid == 0) {
        // Child
        char *const argv[] = { "/home/mikey/dev/Sigma360/src/cmds/watch", "-l", "/home/mikey/dev/Sigma360/src/test", NULL };
        execv(argv[0], argv);
        _exit(127);
    }

    // Parent
    int status;
    while (waitpid(pid, &status, 0) < 0) {
        if (errno != EINTR) {
            return -1;
        }
    }

    if (WIFEXITED(status)) {
        return WEXITSTATUS(status);
    }

    return -1;
}