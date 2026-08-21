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

typedef struct {
    struct ncplane *plane;
    int y;
    int maxy;
} cursor_t;

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

static int create_cursor(struct ncplane *parent, cursor_t *cur, int max) {
    struct ncplane_options nopts = {
        .y = 1,
        .x = 1,
        .rows = 1,
        .cols = (unsigned)ncplane_dim_x(parent) - 2, // inset from parent border
    };
    cur->plane = ncplane_create(parent, &nopts);
    if (!cur->plane) {
        return -1;
    }

    ncplane_set_base(cur->plane, "", 0, 0);
    uint64_t channels = 0;
    ncchannels_set_bg_rgb(&channels, 0x2f7482); // 0x67a176
    ncchannels_set_fg_alpha(&channels, NCALPHA_TRANSPARENT); // let underlying glyph's own fg color show
    ncplane_set_base(cur->plane, "", 0, channels);
    ncplane_perimeter_rounded(cur->plane, 0, channels, 0);

    cur->y = 1;
    cur->maxy = max;
    return 0;
}

static void destroy_cursor(cursor_t *cur) {
    ncplane_destroy(cur->plane);
    cur->plane = NULL;
}

static void move_cursor(cursor_t *cur, int y) {
    cur->y += y;
    if (cur->y < 1) {
        cur->y = cur->maxy;
    }
    if (cur->y > cur->maxy) {
        cur->y = 1;
    }
    ncplane_move_yx(cur->plane, cur->y, 1);
}

void render_courselist(struct ncplane *pane, cJSON *json) {
    int y = 1;
    int printName = (ncplane_dim_x(pane) > 20) ? 1 : 0;
    cJSON *element = NULL;
    cJSON_ArrayForEach(element, json) {
        cJSON *course_code = cJSON_GetObjectItemCaseSensitive(element, "courseCode");
        cJSON *course_name = cJSON_GetObjectItemCaseSensitive(element, "courseName");
        if (cJSON_IsString(course_code) && (course_code->valuestring != NULL) && cJSON_IsString(course_name) && (course_name->valuestring != NULL)) {
            ncplane_putstr_yx(pane, y, 2, course_code->valuestring);
            if (printName) {
                ncplane_putstr_yx(pane, y, strlen(course_code->valuestring) + 2, " - ");
                ncplane_putstr_yx(pane, y, strlen(course_code->valuestring) + 5, course_name->valuestring);
            }
            y++;
        }
    }
}

void render_lecturelist(struct ncplane *pane, cJSON *json, const char *course_url) {
    int y = 1;
    cJSON *course = NULL;
    cJSON_ArrayForEach(course, json) {
        cJSON *url = cJSON_GetObjectItemCaseSensitive(course, "url");
        if (cJSON_IsString(url) && (url->valuestring != NULL) && strcmp(url->valuestring, course_url) == 0) {
            cJSON *lectureAmt = cJSON_GetObjectItemCaseSensitive(course, "lessonCount");
            if (cJSON_IsNumber(lectureAmt)) {
                for (int i = 1; i <= lectureAmt->valueint; i++) {
                    char lectureName[50];
                    snprintf(lectureName, sizeof(lectureName), "Lecture %d", i);
                    ncplane_putstr_yx(pane, y, 2, lectureName);
                    y++;
                }
            }
        }
    }
}

static int layout_panes(struct notcurses *nc, panes_t *panes, cJSON *json) {
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

    render_courselist(panes->current, json);
    render_courselist(panes->parent, json);
    render_lecturelist(panes->preview, json, "22ea00b5-4fc9-463c-b13b-91f17456e1b8");

    return 0;
}

int sigma360_tui(void) {
    cJSON *json = get_json("./src/cmds/courses.json");
    if (!json) {
        fprintf(stderr, "[ERROR] Failed to load JSON data.\n");
        return 1;
    }
    sort_cjson_array(json);

    struct notcurses_options opts = {0};
    opts.flags = NCOPTION_SUPPRESS_BANNERS;

    struct notcurses *nc = notcurses_core_init(&opts, NULL);
    if (!nc) {
        return 1;
    }

    panes_t panes = {0};
    if (layout_panes(nc, &panes, json) != 0) {
        notcurses_stop(nc);
        return 1;
    }
    cursor_t cursor = {0};
    int max = cJSON_GetArraySize(json);
    if (create_cursor(panes.current, &cursor, max) != 0) {
        destroy_panes(&panes);
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
            if (layout_panes(nc, &panes, json) != 0) {
                break;
            }
            notcurses_render(nc);
            continue;
        }
        if (id == 'q' || id == NCKEY_ESC) {
            break;
        }
        if (id =='j' || id == NCKEY_DOWN) {
            move_cursor(&cursor, 1);
            notcurses_render(nc);
            continue;
        }
        if (id =='k' || id == NCKEY_UP) {
            move_cursor(&cursor, -1);
            notcurses_render(nc);
            continue;
        }
    }

    destroy_panes(&panes);
    destroy_cursor(&cursor);
    notcurses_stop(nc);
    cJSON_Delete(json);
    return 0;
}