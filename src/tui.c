#include "tui.h"
#include "navigation.h"
#include "utilities.h"
#include "tui_image.h"
#include "const.h"
#include "fetch.h"
/* #include "saving.h" */

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

bool window_too_small = false;

// ------------------------------------------- //
// Panes                                       //
// ------------------------------------------- //

/**
 * Empties a Pane instance by destroying content and frame panes given they
 * exist.
 * @param pane Pane to empty.
 */
static void destroy_pane(Pane* pane)
{
    if (pane->content != NULL) {
        ncplane_destroy(pane->content);
        pane->content = NULL;
    }
    if (pane->frame != NULL) {
        ncplane_destroy(pane->frame);
        pane->frame = NULL;
    }
}

/**
 * Clears the screen and cleans up each pane. Currently only the preview pane
 * can have images drawn to them, and so only this pane's userptr is handled
 * if existant.
 * @param screen Screen to empty.
 */
static void destroy_screen(Screen* screen)
{
    destroy_pane(&screen->parent);
    destroy_pane(&screen->current);
    imageData* data = ncplane_userptr(&screen->preview);
    if (data) {
        cleanup_image(&screen->preview);
    }
    destroy_pane(&screen->preview);
    destroy_pane(&screen->help);
}

/**
 * Populates a Pane instance with a frame and content ncplanes. Content plane
 * depends on frame details.
 * @param std       The standard plane for the window.
 * @param pane      A pointer to the pane for population.
 * @param frameOpts Options for the frame to be built. Mimimum requirements are
 *      .x, .y, .rows, and .cols populated
 * @param roles     The role the pane will have, see const.h paneroles enum
 *      for roles
 * @returns
 *      BAD_SIZE    given the frameOpts describes too small a frame.
 *      BAD_FRAME   given failure to create frame or contents panes.
 *      GOOD        upon success.
 *
 */
static int make_pane(struct ncplane* std, Pane* pane, 
        struct ncplane_options frameOpts, panerole role)
{
    pane->frame = NULL;
    pane->content = NULL;

    if (frameOpts.rows < MIN_ROWS || frameOpts.cols < MIN_COLS) {
        return BAD_SIZE; // not enough space for a pane
    }
    
    pane->frame = ncplane_create(std, &frameOpts);
    if (pane->frame == NULL) { // Make sure pane was created.
        return BAD_FRAME;
    }

    uint64_t channels = 0;
    ncchannels_set_fg_rgb(&channels, 
            role == ROLE_CURRENT ? COL_BORDER_ACTIVE : COL_BORDER_DIM);
    ncplane_perimeter_rounded(pane->frame, 0, channels, 0);

    struct ncplane_options contentOpts = {
        .y = 1, // Where the top left corner is. Move one in and down 
        .x = 1, // so as to not be overlapping the border
        .rows = frameOpts.rows - 2, // -2 Because of border top and bottom,
        .cols = frameOpts.cols - 2, // and left and right
    };
    pane->content = ncplane_create(pane->frame, &contentOpts);
    if (pane->content == NULL) { // Make sure pane was created.
        destroy_pane(pane);
        return BAD_FRAME;
    }
    return GOOD;
}

/**
 * Generated the panes for the screen, and places them on the screen. Lots
 * of magical numbers. This is where you go to change pane dimenions at any 
 * point.
 * @param nc     The notcurses struct containing the panes that can be drawn.
 * @param screen The screen to be populated with panes.
 * @returns
 *      BAD_SIZE    given the screen size cannot accomodate panes.
 *      exitCode    given any make_pane calls fail, return the exit status of
 *          said call
 *      GOOD        upon success.
 */
static int layout_panes(struct notcurses* nc, Screen* screen) 
{
    struct ncplane* std = notcurses_stdplane(nc);
    unsigned rows;
    unsigned cols;
    ncplane_dim_yx(std, &rows, &cols);

    unsigned parentW = 55;

    if (rows < HELP_ROWS + MIN_ROWS || cols < parentW + (3 * MIN_COLS)) {
        return BAD_SIZE; // not enough space for the columns plus the help bar
    }

    unsigned bodyH = rows - HELP_ROWS;
    unsigned remaining = cols - parentW;
    unsigned currentW = (remaining * 2) / 5;
    unsigned previewW = remaining - currentW;

    if ((exitCode = make_pane(std, &screen->parent, (struct ncplane_plane)
                {.y=0, .x=0, .rows=bodyH, .cols=parentW}, 
                ROLE_PARENT))) { // Build parent contents
        destroy_screen(screen);
        return exitCode;
    }
    if ((exitCode = make_pane(std, &screen->current, (struct ncplane_options)
                {.y=0, .x=(int)parentW, .rows=bodyH, .cols=currentW}, 
                ROLE_CURRENT))) { // Build current contents
        destroy_screen(screen);
        return exitCode;
    }
    if ((exitCode = make_pane(std, &screen->preview, (struct ncplane_options)
                {.y=0, .x=(int)(parentW+currentW), .rows=bodyH, .cols=previewW},
                ROLE_PARENT))) { // Build preview contents
        destroy_screen(screen);
        return exitCode;
    }
    if ((exitCode = make_pane(std, &screen->help, (struct ncplane_options)
                {.y=(int)bodyH, .x=0, .rows=HELP_ROWS, .cols=cols}, 
                ROLE_HELP))) { // Build help contents
        destroy_screen(screen);
        return exitCode;
    }

    return GOOD;
}

// ------------------------------------------- //
// Rendering                                   //
// ------------------------------------------- //

/**
 * Calculates what the top most object in a pane is. Given more objects in a 
 * pane then rows this is non-trivial. Otherwise its as expected.
 * @param cursor A pointer to the program's cursor.
 * @param rows   The number of available rows a pane has.
 */
static void clamp_view(Cursor* cursor, int rows) 
{
    if (rows == 0) {
        return;
    }
    Course* course = get_course(cursor);
    int count = cursor->level ? get_lecCount(cursor) : cursor->coursesCount;
    int sel = cursor->level ? course->lectureSel : cursor->courseSel;
    int* top;
    if (cursor->level) {
        top = get_topLecture(cursor);
    } else {
        top = get_topCourse(cursor);
    }
    if (sel < *top) {
        *top = sel;
    } else if (sel >= *top + rows) {
        *top = sel - rows + 1;
    }
    if (*top > count - rows) {
        *top = count <= rows ? 0 : count - rows;
    }
}

/**
 * Draws the logo to the desired plane. 
 * @param p Plane to be drawn to.
 */
static void draw_logo(struct ncplane* p)
{
    const char* const logo[] = {
        "   _____ _     Welcome to        ____    __   ___  ",
        "  / ____(_)                     |___ \\  / /  / _ \\ ",
        " | (___  _  __ _ _ __ ___   __ _  __) |/ /_ | | | |",
        "  \\___ \\| |/ _` | '_ ` _ \\ / _` ||__ <| '_ \\| | | |",
        "  ____) | | (_| | | | | | | (_| |___) | (_) | |_| |",
        " |_____/|_|\\__, |_| |_| |_|\\__,_|____/ \\___/ \\___/ ",
        "            __/ |                                  ",
        "           |___/                                   ",
        NULL
    };
    ncplane_set_fg_rgb(p, COL_BORDER_ACTIVE);
    int y = 0;
    while (logo[y]) {
        ncplane_putstr_yx(p, y, 0, logo[y]);
        y++;
    } 
    ncplane_set_fg_default(p);
}

/**
 * Draws the courses to the desired plane. Currently studied courses are
 * highlighted with COL_ACTIVE_COURSE (see const.h). Currently selected course's
 * background is highlighted with COL_SEL_BG_ACTIVE (see cont.h).
 * @param p      Plane to be drawn to.
 * @param cursor A pointer to the program's cursor instance.
 * @param role   The role of the plane, be it parent, current, or preview.
 * @param dims   The dimensions of the plane to draw to.
 */
static void draw_courses(struct ncplane* p, Cursor* cursor, 
        panerole_t role, unsigned* dims)
{
    // Might need to cook something in to tell the user no courses available
    int w = (int) dims[1] - 1 >= 1 ? (int) dims[1] - 1 : 1;
    for (int i = cursor->topCourse; i < cursor->coursesCount 
            && (i - cursor->topCourse) < (int) dims[1]; i++) {
        Course course = cursor->courses[i];
        int len = snprintf(NULL, 0, "%s - %s", 
                course.data->courseCode, course.data->courseName);
        char line[len + 1];
        snprintf(line, len + 1 , "%s - %s", 
                course.data->courseCode, course.data->courseName);
        if (i == cursor->courseSel) {
            ncplane_set_bg_rgb(p, (role == ROLE_CURRENT) ? 
                    COL_SEL_BG_ACTIVE : COL_SEL_BG_IDLE);
            ncplane_set_fg_rgb(p, course.data->isActive ? 
                    COL_ACTIVE_COURSE : COL_SEL_FG);
        } else {
            ncplane_set_bg_default(p);
            ncplane_set_fg_rgb(p, course.data->isActive ? 
                    COL_ACTIVE_COURSE : 0xf0f0f0);
        }
        ncplane_printf_yx(p, (int) (i - cursor->topCourse), 0, 
                " %-*.*s", w, w, line);
    }
}

/**
 * Draws the lectures to the desired plane. Currently selected lecture's
 * background is highlighted with COL_SEL_BG_ACTIVE (see cont.h).
 * @param p      Plane to be drawn to.
 * @param cursor A pointer to the program's cursor instance.
 * @param role   The role of the plane, be it parent, current, or preview.
 * @param dims   The dimensions of the plane to draw to.
 */
static void draw_lectures(struct ncplane* p, Course* course, 
        panerole_t role, unsigned* dims)
{
    int w = (int) dims[1] - 1 >= 1 ? (int) dims[1] - 1 : 1;
    for (int i = course->topLecture; i < course->data->lecCount
            && (i - course->topLecture) < (int) dims[0]; i++) {
        int num = course->lectures[i].lectureNum;
        int len = snprintf(NULL, 0, "Lecture %d", num);
        char line[len + 1];
        snprintf(line, len + 1 , "Lecture %d", num);
        if (i == course->lectureSel) {
            ncplane_set_bg_rgb(p, (role == ROLE_CURRENT) ? 
                    COL_SEL_BG_ACTIVE : COL_SEL_BG_IDLE);
            ncplane_set_fg_rgb(p, COL_SEL_FG);
        } else {
            ncplane_set_bg_default(p);
            ncplane_set_fg_rgb(p, COL_TEXT_DEF);
        }
        ncplane_printf_yx(p, (int) (i - course->topLecture), 0, 
                " %-*.*s", w, w, line);
    }
}

/**
 * Draws the plane contents to the display buffer for rendering. 
 * @param p      Plane to be drawn to.
 * @param cursor A pointer to the program's cursor instance.
 * @param role   The role of the plane, be it parent, current, or preview.
 */
static void draw_pane(struct ncplane* p, Cursor* cursor, panerole_t role)
{
    if (!p || !cursor) { // Might need to check for no lectures or courses?
        return;
    }
    ncplane_erase(p);

    unsigned rows = ncplane_dim_y(p);
    unsigned cols = ncplane_dim_x(p);
    if (rows == 0 || cols == 0) {
        return;
    }
    clamp_view(cursor, (int) rows);
    unsigned dims[2] = { rows, cols };
    Course* course = get_course(cursor);

    if (role == ROLE_PARENT) {
        cursor->level ? draw_courses(p, cursor, role, dims) : draw_logo(p);
    } else if (role == ROLE_CURRENT) {
        cursor->level ? draw_lectures(p, course, role, dims) 
            : draw_courses(p, cursor, role, dims);
    } else if (role == ROLE_PREVIEW) {
        if (!cursor->level) {
            draw_lectures(p, course, role, dims);
        }
    }
    ncplane_set_bg_default(p);
    ncplane_set_fg_default(p);
}

/**
 * Draws the keybind help pane to the bottom of screen.
 * @param help A pointer to the program's screen.help Pane. 
 */
static void draw_help(const Pane* help) 
{
    struct ncplane *p = help->content;
    if (!p) {
        return;
    }
    ncplane_erase(p);
 
    const keybind binds[] = {
        { "q/esc",   "quit"  },
        { "enter",   "watch" },
        { "t",       "watch at time"},
        { "s",       "save"  },
        { "h/left",  "back"  },
        { "j/down",  "down"  },
        { "k/up",    "up"    },
        { "l/right", "into"  },
        { "shift+[watch]", "split screen"},
    }
 
    int x = 1;
    for (size_t i = 0; i < sizeof binds / sizeof *binds; i++) {
        int w;
 
        ncplane_set_fg_rgb(p, COL_HELP_KEY);
        w = ncplane_putstr_yx(p, 0, x, binds[i].keys);
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

/**
 * Draws all panes to the screen.
 * @param screen A pointer to the program's screen instance.
 * @param cursor A pointer to the program's cursor instance.
 */
static void draw_all(Screen* screen, Cursor* cursor)
{
    // Draw left (parent) pane
    draw_pane(screen->parent.content, cursor, ROLE_PARENT);
    // Draw middle (active) pane
    draw_pane(screen->current.content, cursor, ROLE_CURRENT);
    // Draw right (preview) pane
    draw_pane(screen->preview.content, cursor, ROLE_PREVIEW);
    draw_help(&screen->help);
}

// ------------------------------------------- //
// Main                                        //
// ------------------------------------------- //

int sigma360_tui(void) {

    int exitCode = GOOD;

    // SETUP ROUTINE

    if ((exitCode = get_cookies())) {
        return exitCode;
    }

    cJSON* json;
    if ((exitCode = read_courses_json(coursesJSON, &json))) {
        return exitCode;
    }
    sort_cjson_array(json);

    char* root;
    if ((exitCode = build_tree(&root))) {
        return exitCode;
    }

    pid_t thumbGetter = fork();
    get_thumbnails(root, &thumbGetter);

    Cursor cursor;
    if (init_cursor(&cursor, json) != 0) {
        cJSON_Delete(json);
        return BAD_CURSOR;
    }

    struct notcurses_options opts = {0};
    opts.flags = NCOPTION_SUPPRESS_BANNERS;

    struct notcurses *nc = notcurses_init(&opts, NULL);
    if (nc == NULL) {
        destruct_cursor(&cursor);
        cJSON_Delete(json);
        return 1;
    }

    Screen screen = { 0 };
    if (layout_panes(nc, &screen) != 0) {
        notcurses_stop(nc);
        cJSON_Delete(json);
        return BAD_PANES;
    }

    draw_all(&screen, &cursor);
    notcurses_render(nc);

    // END OF SETUP ROUTINE

    // USER INTERACTION ROUTINE

    struct ncinput ni;
    while (1) {
        // Block until keyboard input
        uint32_t id = notcurses_get_blocking(nc, &ni);

        if (id == (uint32_t) -1) {
            break; // error
        }
        if (ni.evtype == NCTYPE_RELEASE) {
            continue; // ignore key-up on Kitty-protocol terminals
        }
        //
        // Want to implement a "Are you sure you want to quit" box
        //
        if (id == 'q' || id == NCKEY_ESC) {
            if (!fork()) {
                execlp("rm", "rm", "-rf", root, NULL);
                execlp("rm", "rm", coursesJSON, NULL);
                _exit(BAD);
            }
            
            wait(NULL);
            waitpid(thumbGetter, NULL, 0);
            break; // quiting out
        }

        if (id == NCKEY_RESIZE) {
            unsigned rows, cols;
            if (notcurses_refresh(nc, &rows, &cols) != 0) {
                continue;   // couldn't re-fetch; try again on the next event
            }
        
            destroy_screen(&screen);
            if (layout_panes(nc, &screen) != 0) {
                window_too_small = true;
                continue;
            }
            window_too_small = false;
        
            draw_all(&screen, &cursor);
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
            move_cursor_x(&cursor, 1);
        } else if (id == 'k' || id == NCKEY_UP) {
            move_cursor_x(&cursor, -1);
        } else if (id == 'l' || id == NCKEY_RIGHT) {
            move_cursor_y(&cursor, 1);
        } else if (id == 'h' || id == NCKEY_LEFT) {
            move_cursor_y(&cursor, -1);
        } else if (id == NCKEY_ENTER && !ni.shift) {
            if (cursor.level > 0 && get_lecCount(&cursor) > 0) {
                // Grab the lecture's /tmp directory name
                char* dir = build_dir(root, 
                        get_courseKey(&cursor), (int) get_currLec(&cursor) + 1);
                // Check if there's anything in it.
                if (!is_lec_downloaded(dir)) {
                    struct ncplane* box = NULL;
                    build_download_box(nc, &box);
                    watch_lec(dir, false, "00;00;00");
                    if (box) { 
                        ncplane_destroy(box);
                    }
                } else {
                    watch_lec(dir, false, "00;00;00");
                }
                free(dir);
            }
        } else if (id == 's') {
            /* preview_image_clear(); */
            /* sigma360_tui_save(nc, &cursor, root); */
        } else if (id == NCKEY_ENTER && ni.shift) {
            dispatch_watch(&cursor, root, true, "00;00;00");
        } else if (id == 't') {
            char* timestamp;
            if (!get_timestamp(nc, &timestamp)) {
                dispatch_watch(&cursor, root, false, timestamp);
            } else {
                // exit silently if escaped from
            }
        } else if (id == 'T') {
            char* timestamp;
            if (!get_timestamp(nc, &timestamp)) {
                dispatch_watch(&cursor, root, true, timestamp);
            } else {
                // exit silently if escaped from
            }
        } else {
            continue; // some unbound key; no redraw required
        }

        // Generate preview image. This doesn't work well, see TODO
        if (cursor.level == DEEPEST_LEVEL) {
            char* dir = build_dir(root, 
                    get_courseKey(&cursor), (int) get_currLec(&cursor) + 1);
            expand_path(&dir);
            char* thumbnail = buildArgs(dir, "t.jpg");
            free(dir);
            if (access(temp, F_OK) == 0) { // man access
                preview_image_show(&screen.preview, thumbnail);
            } else {
                preview_image_show(&screen.preview, defaultImage);
            }
            free(thumbnail);
        }
        draw_all(&screen, &cursor);
        notcurses_render(nc);
    }

    destroy_screen(&screen);
    notcurses_stop(nc);
    cJSON_Delete(json);
    destruct_cursor(cursor);
    return GOOD;
}

void dispatch_watch(Cursor* cursor, char* root, bool ss, char* time)
{
    if (cursor->level > 0 && get_lecCount(cursor) > 0) {
        char* dir = build_dir(root, 
                get_courseKey(cursor), (int) get_currLec(cursor) + 1);
        watch_lec(dir, ss, time);
        free(dir);
    }
}

int watch_lec(char* dir, bool split, char* time)
{
    pid_t pid = fork();

    if (pid == 0) {
        char* argv[]
            = { watch, "-l", dir, "-t", time, split ? "-s" : NULL, NULL };
        execv(argv[0], argv);
        _exit(BAD_CMD_EXEC);
    }
    // Parent
    int status;
    waitpid(pid, &status, 0);
    if (WIFEXITED(status)) {
        return WEXITSTATUS(status);
    }
    return GOOD;
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
                "Enter a start time for the lecture (HH;MM;SS): ");
        ncplane_putstr_yx(popup, 3, 2, *timestamp);
        preview_image_clear();
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
        } else if ((key >= '0' && key <= '9') || key == ';') {
            *timestamp = realloc(*timestamp, ++size * sizeof(char));
            (*timestamp)[size - 2] = key;
            (*timestamp)[size - 1] = '\0';
        }
    }

    ncplane_destroy(popup);
    return GOOD;
}

void build_download_box(struct notcurses* nc, struct ncplane** box)
{
    struct ncplane *std = notcurses_stdplane(nc);
    unsigned r, c;
    ncplane_dim_yx(std, &r, &c);
    unsigned bw = (c > 40) ? 40 : c;
    struct ncplane_options bo = {
        .y = (int) (r - 3) / 2,
        .x = (int) (c - bw) / 2, 
        .rows = 3,
        .cols = bw,
    };
    *box = ncplane_create(std, &bo);
    if (box) {
        uint64_t ch = 0;
        ncchannels_set_fg_rgb(&ch, COL_SEL_FG);
        ncchannels_set_bg_rgb(&ch, 0x000000);
        ncplane_set_base(*box, " ", 0, ch);
        ncchannels_set_fg_rgb(&ch, COL_BORDER_ACTIVE);
        ncplane_perimeter_rounded(*box, 0, ch, 0);
        ncplane_set_fg_rgb(*box, COL_HELP_DESC);
        ncplane_set_bg_rgb(*box, 0x000000);
        ncplane_putstr_yx(*box, 1, 2, "downloading...");
        preview_image_clear();
        notcurses_render(nc);
    }
}
