#include "tui.h"
#include "nav.h"
#include "utilities.h"
#include "tui_image.h"

#include <notcurses/nckeys.h>
#include <notcurses/notcurses.h>
#include <cjson/cJSON.h>

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <time.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <limits.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <fcntl.h>

#define COL_BORDER_DIM    0x6272a4u
#define COL_BORDER_ACTIVE 0xbd93f9u
#define COL_SEL_BG_ACTIVE 0x2f7482u
#define COL_SEL_BG_IDLE   0x44475au
#define COL_SEL_FG        0xf8f8f2u
#define COL_MODAL_BG      0x282a36u
#define COL_HELP_KEY      0xbd93f9u
#define COL_HELP_DESC     0x91bbffu

#define DETAILS_MIN_COLS 10
#define HELP_ROWS 3

bool window_too_small = false;

typedef struct {
    struct ncplane *frame;
    struct ncplane *content;
} pane_t;

typedef struct {
    pane_t parent;
    pane_t current;
    pane_t preview;
    pane_t help;
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
    destroy_pane(&panes->help);
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

    unsigned parentw = 55;

    if (rows < HELP_ROWS + 3 || cols < parentw + 8) {
        return -1; // not enough space for the columns plus the help bar
    }

    unsigned bodyh = rows - HELP_ROWS;
    unsigned remaining = cols - parentw;
    unsigned currentw = (remaining * 2) / 5;
    unsigned previeww = remaining - currentw;

    if (make_pane(std, &panes->parent,  0, 0, bodyh, parentw,  COL_BORDER_DIM) != 0 ||
        make_pane(std, &panes->current, 0, (int)parentw, bodyh, currentw, COL_BORDER_ACTIVE) != 0 ||
        make_pane(std, &panes->preview, 0, (int)(parentw + currentw), bodyh, previeww, COL_BORDER_DIM) != 0 ||
        make_pane(std, &panes->help, (int)bodyh, 0, HELP_ROWS, cols, COL_BORDER_DIM) != 0) {
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

static void draw_parent_pane(struct ncplane *p, nav_t *nav) {
    if (p == NULL) return;
    ncplane_erase(p);

    if (nav->depth == 0) {
        static const char *art[] = {
            "   _____ _     Welcome to        ____    __   ___  ",
            "  / ____(_)                     |___ \\  / /  / _ \\ ",
            " | (___  _  __ _ _ __ ___   __ _  __) |/ /_ | | | |",
            "  \\___ \\| |/ _` | '_ ` _ \\ / _` ||__ <| '_ \\| | | |",
            "  ____) | | (_| | | | | | | (_| |___) | (_) | |_| |",
            " |_____/|_|\\__, |_| |_| |_|\\__,_|____/ \\___/ \\___/ ",
            "            __/ |                                  ",
            "           |___/                                   ",
        };
        ncplane_set_fg_rgb(p, COL_BORDER_ACTIVE);
        for (int y = 0; y < 8; y++) {
            ncplane_putstr_yx(p, y, 1, art[y]);
        }
        ncplane_set_fg_default(p);
        return;
    };

    draw_list(p, nav_parent(nav), ROLE_PARENT);
}

static void draw_help(const pane_t *help) {
    struct ncplane *p = help->content;
    if (p == NULL) {
        return;
    }
    ncplane_erase(p);
 
    static const struct { const char *key; const char *desc; } binds[] = {
        { "q/esc",   "quit"  },
        { "enter",   "watch" },
        { "t",       "watch at time"},
        { "s",       "save"  },
        { "h/left",  "back"  },
        { "j/down",  "down"  },
        { "k/up",    "up"    },
        { "l/right", "into"  },
        { "shift+[watch]", "split screen"},
    };
 
    int x = 1;
    for (size_t i = 0; i < sizeof binds / sizeof *binds; i++) {
        int w;
 
        ncplane_set_fg_rgb(p, COL_HELP_KEY);
        w = ncplane_putstr_yx(p, 0, x, binds[i].key);
        if (w < 0) {
            break; // ran out of pane width
        }
        x += w;
 
        ncplane_set_fg_rgb(p, COL_HELP_DESC);
        w = ncplane_printf_yx(p, 0, x, " %s   ", binds[i].desc);
        if (w < 0) {
            break;
        }
        x += w;
    }
 
    ncplane_set_fg_default(p);
}

static void draw_all(panes_t *panes, nav_t *nav) {
    draw_parent_pane(panes->parent.content, nav);
    draw_list(panes->current.content, nav_current(nav), ROLE_CURRENT);
    draw_list(panes->preview.content, nav_preview(nav), ROLE_PREVIEW);
    draw_help(&panes->help);
}

// ------------------------------------------- //
// Main                                        //
// ------------------------------------------- //

static int sigma360_tui_watch(char* dir, bool split, char* time);
static int sigma360_tui_save(struct notcurses *nc, nav_t *nav, const char *root);
char* build_dir(char* root, char* url, int lectureNum);
void dispatch_watch(nav_t* nav, char* root, bool ss, char* time);
int get_timestamp(struct notcurses* nc, char** timestamp);

int sigma360_tui(void) {
    char *script;
    if (findcwd(&script) != 0) {
        return -1;
    }
    strcat(script, "/src/cmds/girlscout.py");

    pid_t pid = fork();
    if (pid < 0) {
        exit(EXIT_FAILURE);
    }
    if (pid == 0) {
        // Child process
        execlp("python3", "python3", script, (char *)NULL);
        // execlp only returns on failure
        perror("execlp failed");
        exit(EXIT_FAILURE);
    } else {
        // Parent process
        int status;
        waitpid(pid, &status, 0);
        if (WIFEXITED(status)) {
            printf("Child exited with status %d\n", WEXITSTATUS(status));
        } else {
            printf("Child did not exit normally\n");
        }
    }

    cJSON *json = get_courses_json("./courses.json");
    if (!json) {
        fprintf(stderr, "[ERROR] Failed to load JSON data.\n");
        return 1;
    }
    sort_cjson_array(json);

    char* root;
    if (build_tree(&root)) {
        fprintf(stderr, "[ERROR] Failed to construct tmp tree.\n");
        return 1;
    }

    // THUMBNAILS FETCHER HERE
    pid_t thumb = fork();
    if (!thumb) {
        char* cmd;
        findcwd(&cmd);
        strcat(cmd, "/src/cmds/fetcher.py");
        execlp("python3", "python3", cmd, "--thumbnail", root, NULL);
        exit(-1);
    }

    nav_t nav;
    if (nav_init(&nav, json) != 0) {
        fprintf(stderr, "[ERROR] Failed to build navigation tree.\n");
        cJSON_Delete(json);
        return 1;
    }

    struct notcurses_options opts = {0};
    opts.flags = NCOPTION_SUPPRESS_BANNERS;

    struct notcurses *nc = notcurses_init(&opts, NULL);
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
            if (!fork()) {
                execlp("rm", "rm", "-rf", root, NULL);
            }
            wait(NULL);
            waitpid(thumb, NULL, 0);
            break; // quiting out
        }

        if (id == NCKEY_RESIZE) {
            unsigned rows, cols;
            if (notcurses_refresh(nc, &rows, &cols) != 0) {
                continue;   // couldn't re-fetch; try again on the next event
            }
        
            destroy_panes(&panes);
            if (layout_panes(nc, &panes) != 0) {
                window_too_small = true;
                continue;
            }
            window_too_small = false;
        
            draw_all(&panes, &nav);
            notcurses_render(nc);
            continue;
        }
        if (window_too_small) {
            struct ncplane *std = notcurses_stdplane(nc);
            ncplane_erase(std);
            ncplane_putstr_yx(std, 0, 0, "terminal too small");
            notcurses_render(nc);
            continue;
        }

        else if (id == 'j' || id == NCKEY_DOWN) {
            nav_move(&nav, 1);
        } else if (id == 'k' || id == NCKEY_UP) {
            nav_move(&nav, -1);
        } else if (id == 'l' || id == NCKEY_RIGHT) {
            nav_descend(&nav);
        } else if (id == 'h' || id == NCKEY_LEFT) {
            nav_ascend(&nav);
        } else if (id == NCKEY_ENTER && !ni.shift) {
            if (!nav_descend(&nav)) {
                list_t *l = nav_current(&nav);
                if (nav.depth > 0 && l->count > 0) {
                    char* dir = build_dir(root, nav.path[nav.depth]->url, (int)l->sel + 1);

                    if (is_dir_empty(dir)) {
                        struct ncplane *std = notcurses_stdplane(nc);
                        unsigned r, c;
                        ncplane_dim_yx(std, &r, &c);
                        unsigned bw = (c > 40) ? 40 : c;
                        struct ncplane_options bo = {
                            .y = (int)(r - 3) / 2, .x = (int)(c - bw) / 2, .rows = 3, .cols = bw,
                        };
                        struct ncplane *box = ncplane_create(std, &bo);
                        if (box) {
                            uint64_t ch = 0;
                            ncchannels_set_fg_rgb(&ch, COL_SEL_FG);
                            ncchannels_set_bg_rgb(&ch, 0x000000);
                            ncplane_set_base(box, " ", 0, ch);
                            ncchannels_set_fg_rgb(&ch, COL_BORDER_ACTIVE);
                            ncplane_perimeter_rounded(box, 0, ch, 0);
                            ncplane_set_fg_rgb(box, COL_HELP_DESC);
                            ncplane_set_bg_rgb(box, 0x000000);
                            ncplane_putstr_yx(box, 1, 2, "downloading...");
                            ncplane_move_top(box);
                            notcurses_render(nc);
                        }
                        sigma360_tui_watch(dir, false, "00:00:00");

                        if (box) ncplane_destroy(box);
                    } else {
                        sigma360_tui_watch(dir, false, "00:00:00");
                    }
                    free(dir);
                }
            }
        } else if (id == 's') {
            sigma360_tui_image_clear();
            sigma360_tui_save(nc, &nav, root);
        } else if (id == NCKEY_ENTER && ni.shift) {
            dispatch_watch(&nav, root, true, "00:00:00");
        } else if (id == 't') {
            char* timestamp;
            if (!get_timestamp(nc, &timestamp)) {
                dispatch_watch(&nav, root, false, timestamp);
            } else {
                // exit silently if escaped from
            }
        } else if (id == 'T') {
            char* timestamp;
            if (!get_timestamp(nc, &timestamp)) {
                dispatch_watch(&nav, root, true, timestamp);
            } else {
                // exit silently if escaped from
            }
        } else {
            continue; // some unbound key; no redraw required
        }

        if (nav.depth > 0 && nav.depth > 0 && nav_current(&nav)->count > 0) {
            char imgfile[4096];
            char* dir = build_dir(root, nav.path[nav.depth]->url,
                    (int)nav_current(&nav)->sel + 1);
            char temp[4096];
            sprintf(temp, "%s/t.jpg", dir);
            if (access(temp, F_OK) == 0) {
                sigma360_tui_image_show(panes.preview.content, temp);
            } else {
                sprintf(imgfile, "./previewless.jpg");
                sigma360_tui_image_show(panes.preview.content, imgfile);
            }
            free(dir);
        } else {
            sigma360_tui_image_clear();
        }
        notcurses_render(nc);

        draw_all(&panes, &nav);
        notcurses_render(nc);
    }

    destroy_panes(&panes);
    notcurses_stop(nc);
    nav_free(&nav);
    cJSON_Delete(json);
    return 0;
}

void dispatch_watch(nav_t* nav, char* root, bool ss, char* time)
{
    if (!nav_descend(nav)) {
        list_t *l = nav_current(nav);
        if (nav->depth > 0 && l->count > 0) {
            char* dir = build_dir(root, nav->path[nav->depth]->url, 
                    (int)l->sel + 1);
            sigma360_tui_watch(dir, ss, time);
            free(dir);
        }
    }
}

static int sigma360_tui_watch(char* dir, bool split, char* time)
{
    pid_t pid = fork();
    if (pid < 0) {
        return -1;
    }

    if (pid == 0) {
        // Child
        int devnull = open("/dev/null", O_RDWR);
        if (devnull < 0) {
            _exit(127);
        }
        dup2(devnull, STDOUT_FILENO);
        /* dup2(devnull, STDERR_FILENO); */
        if (devnull > STDERR_FILENO) {
            close(devnull);
        }

        char* cmd;
        findcwd(&cmd);
        strcat(cmd, "/src/cmds/watch");
        
        char* const argv[] = { cmd, "-l", dir, "-t", time, split ? "-s" : NULL, NULL };
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

// ------------------------------------------- //
// Saving                                      //
// ------------------------------------------- //

// Path of the lecture the current pane has highlighted, as
// "<root>/<sectionId>/Lecture<n>". NULL when the pane is showing courses
// rather than lectures, so there is nothing to save.
static char *selected_lecture_dir(nav_t *nav, const char *root) {
    if (root == NULL || nav->depth < 1) {
        return NULL;
    }
    list_t *l = nav_current(nav);
    if (l->count == 0) {
        return NULL;
    }
    const char *section = nav->path[nav->depth]->url;
    if (section == NULL) {
        return NULL;
    }

    int len = snprintf(NULL, 0, "%s/%s/Lecture%d", root, section, (int)l->sel + 1);
    if (len < 0) {
        return NULL;
    }
    char *dir = malloc((size_t)len + 1);
    if (dir != NULL) {
        snprintf(dir, (size_t)len + 1, "%s/%s/Lecture%d", root, section, (int)l->sel + 1);
    }
    return dir;
}

// The fetcher always writes audio.mp4 and v1.mp4 (v2.mp4 only exists for
// dual-screen recordings), so both being present means the lecture is on disk.
// Checking the files rather than is_dir_empty() keeps a downloaded thumbnail
// from passing for a downloaded lecture.
static bool lecture_downloaded(const char *dir) {
    char path[PATH_MAX];

    snprintf(path, sizeof path, "%s/v1.mp4", dir);
    if (access(path, R_OK) != 0) {
        return false;
    }
    snprintf(path, sizeof path, "%s/audio.mp4", dir);
    return access(path, R_OK) == 0;
}

// Downloads the lecture into dir, the same way src/cmds/watch does.
static int fetch_lecture(const char *dir) {
    char *script;
    if (findcwd(&script) != 0) {
        return -1;
    }
    strcat(script, "/src/cmds/fetcher.py");

    pid_t pid = fork();
    if (pid < 0) {
        free(script);
        return -1;
    }

    if (pid == 0) {
        // Child. The fetcher chatters on stdout/stderr, which would land on top
        // of the TUI.
        int devnull = open("/dev/null", O_RDWR);
        if (devnull >= 0) {
            dup2(devnull, STDOUT_FILENO);
            dup2(devnull, STDERR_FILENO);
            if (devnull > STDERR_FILENO) {
                close(devnull);
            }
        }
        execlp("python3", "python3", script, "--watch", dir, NULL);
        _exit(127);
    }

    // Parent
    free(script);
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

// Copies the lecture's media to dest. The trailing "/." copies the contents of
// the lecture directory, which works whether or not dest already exists.
static int copy_lecture(const char *dir, const char *dest) {
    char from[PATH_MAX];
    if (snprintf(from, sizeof from, "%s/.", dir) >= (int)sizeof from) {
        return -1;
    }

    pid_t pid = fork();
    if (pid < 0) {
        return -1;
    }

    if (pid == 0) {
        // Child
        int devnull = open("/dev/null", O_RDWR);
        if (devnull >= 0) {
            dup2(devnull, STDOUT_FILENO);
            dup2(devnull, STDERR_FILENO);
            if (devnull > STDERR_FILENO) {
                close(devnull);
            }
        }
        execlp("cp", "cp", "-r", from, dest, NULL);
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

// Replaces the dialog's interior with a single line of text.
static void modal_message(struct notcurses *nc, struct ncplane *box, const char *msg) {
    unsigned rows = ncplane_dim_y(box);
    unsigned cols = ncplane_dim_x(box);

    ncplane_set_fg_rgb(box, COL_HELP_DESC);
    ncplane_set_bg_rgb(box, COL_MODAL_BG);
    for (unsigned y = 1; y + 1 < rows; y++) {
        ncplane_printf_yx(box, (int)y, 1, "%*s", (int)cols - 2, "");
    }
    ncplane_printf_yx(box, 1, 2, "%.*s", (int)cols - 4, msg);

    notcurses_render(nc);
}

// Same, but holds the dialog open until the user has read it.
static void modal_wait(struct notcurses *nc, struct ncplane *box, const char *msg) {
    // The precision leaves room for the suffix no matter how long msg is.
    char line[512];
    snprintf(line, sizeof line, "%.*s  (any key)", (int)(sizeof line - 12), msg);
    modal_message(nc, box, line);

    struct ncinput ni;
    for (;;) {
        uint32_t id = notcurses_get_blocking(nc, &ni);
        if (id == (uint32_t)-1 || ni.evtype != NCTYPE_RELEASE) {
            break;
        }
    }
}

// Downloads the highlighted lecture if it isn't already on disk, then copies it
// to dest. Progress goes into the save dialog, which the caller still owns.
static int save_lecture(struct notcurses *nc, struct ncplane *box, nav_t *nav,
                        const char *root, const char *dest) {
    char *dir = selected_lecture_dir(nav, root);
    if (dir == NULL) {
        modal_wait(nc, box, "no lecture highlighted");
        return -1;
    }

    if (!lecture_downloaded(dir)) {
        modal_message(nc, box, "downloading lecture...");
        if (fetch_lecture(dir) != 0 || !lecture_downloaded(dir)) {
            modal_wait(nc, box, "download failed");
            free(dir);
            return -1;
        }
    }

    char msg[512];
    snprintf(msg, sizeof msg, "saving to %s...", dest);
    modal_message(nc, box, msg);

    int rc = 0;
    if (copy_lecture(dir, dest) != 0) {
        snprintf(msg, sizeof msg, "could not save to %s", dest);
        rc = -1;
    } else {
        snprintf(msg, sizeof msg, "saved to %s", dest);
    }
    modal_wait(nc, box, msg);

    free(dir);
    return rc;
}

static char *save_lecture_name(nav_t *nav);

static int sigma360_tui_save(struct notcurses *nc, nav_t *nav, const char *root) {
    struct ncplane *std = notcurses_stdplane(nc);
    unsigned rows, cols;
    ncplane_dim_yx(std, &rows, &cols);

    unsigned boxh = 5;
    unsigned boxw = (cols > 60) ? 60 : cols;
    if (rows < boxh || boxw < 20) {
        return -1; // no room for the dialog
    }

    struct ncplane_options bopts = {
        .y = (int)(rows - boxh) / 2,
        .x = (int)(cols - boxw) / 2,
        .rows = boxh,
        .cols = boxw,
    };
    struct ncplane *box = ncplane_create(std, &bopts);
    if (box == NULL) {
        return -1;
    }

    // An opaque base cell, otherwise the panes underneath show through.
    uint64_t base = 0;
    ncchannels_set_fg_rgb(&base, COL_SEL_FG);
    ncchannels_set_bg_rgb(&base, COL_MODAL_BG);
    ncplane_set_base(box, " ", 0, base);

    uint64_t border = 0;
    ncchannels_set_fg_rgb(&border, COL_BORDER_ACTIVE);
    ncchannels_set_bg_rgb(&border, COL_MODAL_BG);
    ncplane_perimeter_rounded(box, 0, border, 0);

    ncplane_set_fg_rgb(box, COL_HELP_DESC);
    ncplane_set_bg_rgb(box, COL_MODAL_BG);
    ncplane_putstr_yx(box, 1, 2, "save as:  (enter to confirm, esc to cancel)");

    struct ncplane_options ropts = {
        .y = 2, .x = 2, .rows = 1, .cols = boxw - 4,
    };
    struct ncplane *rp = ncplane_create(box, &ropts);
    if (rp == NULL) {
        ncplane_destroy(box);
        return -1;
    }

    struct ncreader_options rdopts = {0};
    ncchannels_set_fg_rgb(&rdopts.tchannels, COL_SEL_FG);
    ncchannels_set_bg_rgb(&rdopts.tchannels, COL_MODAL_BG);
    rdopts.flags = NCREADER_OPTION_CURSOR | NCREADER_OPTION_HORSCROLL;

    // ncreader takes ownership of rp; ncreader_destroy frees it.
    struct ncreader *rd = ncreader_create(rp, &rdopts);
    if (rd == NULL) {
        ncplane_destroy(rp);
        ncplane_destroy(box);
        return -1;
    }

    bool accepted = false;
    struct ncinput ni;
    for (;;) {
        notcurses_render(nc);

        uint32_t id = notcurses_get_blocking(nc, &ni);
        if (id == (uint32_t)-1) {
            break;
        }
        if (ni.evtype == NCTYPE_RELEASE) {
            continue;
        }
        if (id == NCKEY_ESC) {
            break;
        }
        if (id == NCKEY_ENTER) {
            accepted = true;
            break;
        }
        ncreader_offer_input(rd, &ni);
    }

    char *name = NULL;
    // ncreader_destroy frees rp; the dialog itself stays up to report progress.
    ncreader_destroy(rd, accepted ? &name : NULL);
    notcurses_cursor_disable(nc);

    int rc = 1; // cancelled
    if (accepted && name != NULL && name[0] != '\0') {
        rc = save_lecture(nc, box, nav, root, name);
        
        char video[PATH_MAX], audio[PATH_MAX], out[PATH_MAX];

        snprintf(video, sizeof video, "%s/v1.mp4", name);
        snprintf(audio, sizeof audio, "%s/audio.mp4", name);
        snprintf(out, sizeof out, "%s/%s.mp4", name, save_lecture_name(nav));

        char *args[] = {
            "ffmpeg", "-nostdin", "-y",
            "-i", video,
            "-i", audio,
            "-map", "0:v:0",
            "-map", "1:a:0",
            "-c", "copy",
            "-shortest",
            out,
            NULL
        };

        pid_t pid = fork();
        if (pid < 0) return false;

        if (!pid) { // child
            int fd = open("/dev/null", O_RDWR);
            if (fd >= 0) {
                dup2(fd, STDIN_FILENO);
                dup2(fd, STDOUT_FILENO);
                dup2(fd, STDERR_FILENO);
                if (fd > STDERR_FILENO) close(fd);
            }
            execvp("ffmpeg", args);
            _exit(127);
        }

        int status;
        while (waitpid(pid, &status, 0) < 0 && errno == EINTR);
    }
    ncplane_destroy(box);
    free(name);
    return rc;
}

char* build_dir(char* root, char* url, int lectureNum)
{
    char* dir = strdup(root);
    expand_path(&dir);
    dir = buildArgs(dir, url);
    expand_path(&dir);
    char* lecture = buildLec("Lecture%d", lectureNum);
    dir = buildArgs(dir, lecture);
    return dir;
}

static char *save_lecture_name(nav_t *nav) {
    if (nav->depth < 1) {
        return NULL;
    }
    const entry_t *course  = nav->path[nav->depth];
    const entry_t *lecture = nav_selected(nav);   // NULL when the list is empty
    if (lecture == NULL || course->label == NULL || lecture->label == NULL) {
        return NULL;
    }

    int len = snprintf(NULL, 0, "%s_%s", course->label, lecture->label);
    if (len < 0) {
        return NULL;
    }
    char *s = malloc((size_t)len + 1);
    if (s != NULL) {
        snprintf(s, (size_t)len + 1, "%s_%s", course->label, lecture->label);
    }
    return s;
}

// ------------------------------------------- //
//  Timestamp grabbing                         //
// ------------------------------------------- //

struct ncplane* build_popup(struct notcurses* nc, int rows, int cols)
{
    struct ncplane* stdplane = notcurses_stdplane(nc);
    unsigned planeRows;
    unsigned planeCols;
    ncplane_dim_yx(stdplane, &planeRows, &planeCols);

    int x = ((int)planeCols - cols) / 2;
    int y = ((int)planeRows - rows) / 2;

    struct ncplane_options nopts = {
        .x = x,
        .y = y,
        .rows = (unsigned)rows,
        .cols = (unsigned)cols,
    };
    struct ncplane* popup = ncplane_create(stdplane, &nopts);

    ncplane_set_bg_rgb8(popup, 0, 0, 0);
    ncplane_set_fg_rgb8(popup, 255, 255, 255);
    ncplane_set_base(popup, " ", 0, ncplane_channels(popup));

    return popup;
}

int get_timestamp(struct notcurses* nc, char** timestamp)
{
    // rows = 5, cols = 50. Adjustable to desired window size
    struct ncplane* popup = build_popup(nc, 5, 50);
    
    int size = 1;
    *timestamp = malloc(sizeof(char));
    (*timestamp)[size - 1] = '\0';

    while(true) {
        // Render pane
        ncplane_erase(popup);
        ncplane_perimeter_rounded(popup, 0, 0, 0); // border
        ncplane_putstr_yx(popup, 1, 2, 
                "Enter a start time for the lecture (HH:MM:SS): ");
        ncplane_putstr_yx(popup, 3, 2, *timestamp);
        ncplane_move_top(popup);
        notcurses_render(nc);

        struct ncinput ni;
        uint32_t key = notcurses_get_blocking(nc, &ni);

        if (ni.evtype == NCTYPE_RELEASE || ni.evtype == NCTYPE_REPEAT) {
            continue;
        }

        if (key == NCKEY_ENTER) {
            /* *timestamp = realloc(*timestamp, ++size * sizeof(char)); */
            /* (*timestamp)[size - 1] = '\0'; */
            break;
        } else if (key == NCKEY_ESC || key == 'q') {
            free(*timestamp);
            ncplane_destroy(popup);
            return -1;
        } else if (key == NCKEY_BACKSPACE) {
            if (size > 1) {
                (*timestamp)[--size - 1] = '\0';
            } 
        } else if ((key >= '0' && key <= '9') || (key == ':')) {
            *timestamp = realloc(*timestamp, ++size * sizeof(char));
            (*timestamp)[size - 2] = key;
            (*timestamp)[size - 1] = '\0';
        }
    }

    ncplane_destroy(popup);
    return 0;
}
