#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

#include <notcurses/nckeys.h>
#include <notcurses/notcurses.h>

#include "navigation.h"
#include "utilities.h"
#include "const.h"

#define ESCAPE     -1

static void cleanup_saving(char* strs[], struct ncplane* box)
{
    while (strs[0]) {
        free(strs);
    }
    ncplane_destroy(box);
}

static int read_popup_input(struct notcurses* nc, struct ncreader* reader,
    char** result)
{
    struct ncinput input;
    while (1) {
        notcurses_render(nc);

        uint32_t id = notcurses_get_blocking(nc, &input);
        if (id == (uint32_t) - 1) {
            break;
        }
        if (input.evtype == NCTYPE_RELEASE) {
            continue;
        }
        if (id == NCKEY_ESC) {
            break;
        }
        if (id == NCKEY_ENTER) {
            return ESCAPE;
        }
        ncreader_offer_input(reader, &input);
    }
    ncreader_destroy(reader, result);
    /* notcurses_cursor_disable(nc); */
    return GOOD;
}

static int get_save_path(struct notcurses* nc, struct ncplane* box,
        char** path)
{
    int exitCode = GOOD;
    ncplane_putstr_yx(box, 1, 2, "Enter save path:");

    struct ncreader* reader;
    struct ncplane_options options = {
        .x = 2,
        .y = 2,
        .rows = 1,
        .cols = SAVE_BOX_W - 4
    };
    if ((exitCode = build_reader(box, &reader, options))) {
        return exitCode; 
    }

    // Set up reader plane and reader
    exitCode = read_popup_input(nc, reader, path);
    ncreader_destroy(reader, NULL);
    return GOOD;
}

int validateMsg(struct ncplane* box, char* msg, char** newMsg)
{
    // Note - 2 as we have a border.
    unsigned height = ncplane_dim_y(box) - 2;
    unsigned width = ncplane_dim_x(box) - 2;

    int size = 0;
    *newMsg = malloc(sizeof(char));
    for (int i = 0; i < strlen(msg) && i < height * width; i++) {
        if (i % (int) (width - 1) == 0) {
            *newMsg = realloc(*newMsg, ++size * sizeof(char));
            (*newMsg)[size - 1] = '\n';
        }
        *newMsg = realloc(*newMsg, ++size * sizeof(char));
        (*newMsg)[size - 1] = msg[i];
    }
    *newMsg = realloc(*newMsg, ++size * sizeof(char));
    (*newMsg)[size - 1] = '\0';
    return GOOD;
}

static int draw_savebox(struct notcurses* nc, struct ncplane** box, char* msg)
{
    int exitCode = GOOD;
    if (!*box) {
        if ((exitCode = build_popup(nc, *box, SAVE_BOX_W, SAVE_BOX_H))) {
            return exitCode;
        }
        ncplane_set_scrolling(*box, true);
    }
    ncplane_erase(*box);

    ncplane_set_bg_rgb8(*box, 0, 0, 0);
    ncplane_set_fg_rgb8(*box, 255, 255, 255);

    char* newMsg;
    validate_msg(*box, msg, &newMsg);
    ncplane_putstr_yx(*box, 1, 2, newMsg);
    free(newMsg);
    notcurses_render(nc);
    return exitCode
}

static int draw_savebox_wait(struct notcurses* nc, 
        struct ncplane** box, char* msg)
{
    int exitCode = GOOD;
    if ((exitCode = draw_savebox(nc, box, msg))) {
        return exitCode;
    }

    // Wait until any keyboard input
    struct ncinput input;
    while (true) {
        uint32_t id = notcurses_get_blocking(nc, &input);
        if (id == (unint32) -1 || input.evtype != NCTYPE_RELEASE) {
            break;
        }
    }
}

static int stitch_lecture(char* desination)
{

}

static int copy_lecture(char* lecture, char* destination)
{
    pid_t pid = fork();
    if (pid < 0) {
        return BAD;
    }
    
    if (!pid) {
       execlp("cp", "cp", "-r", lecture, destination, NULL);
       _exit(BAD);
    }
    int status;
    waitpid(pid, &status, 0);
    if (WIFEXITED(status)) {
        if (WEXITSTATUS(status)) {
            return BAD_SAVE;
        }
    } else {
        return BAD_SAVE;
    }

    int exitCode = stitch_lecture(destination);
    return exitCode;
}

int save_lecture(struct notcurses* nc, Cursor* cursor, const char* root)
{
    int exitCode = GOOD;
    char* savePath;

    struct notcurses* box = NULL;
    if ((exitCode = draw_savebox(nc, &box, "Enter save path:"))) {
        return exitCode;
    }

    if ((exitCode = get_save_path(nc, &savePath))) {
        return exitCode;
    }
    char* key = get_courseKey(cursor);
    int lecNum = get_currLec(cursor);
    char* lecDir = build_dir(root, key, lecNum);
    
    if (!is_lec_downloaded(lecDir)) {
        draw_savebox(nc, &box, "Downloading lecture.");
        if ((exitCode = get_lecture(lecDir))) {
            // NEED SOME ERROR HANDLING HERE WHEN FAILS
            cleanup_saving((char*[]) {lecDir, savePath, NULL}, box);
            return exitCode;
        }
    }

    draw_savebox(nc, &box, "Saving lecture...");
    char* resultMsg;
    if ((exitCode = copy_lecture(lecDir, savePath))) {
        int len = snprintf(NULL, 0, "Could not save to %s", savePath);
        resultMsg = malloc(++len * sizeof(char));
        snprintf(resultMsg, len, "Could not save to %s", savePath);
    } else {
        int len = snprintf(NULL, 0, "Could not save to %s", savePath);
        resultMsg = malloc(++len * sizeof(char));
        snprintf(resultMsg, len, "Could not save to %s", savePath);
    }
    draw_savebox_wait(nc, &box, resultMsg);

    cleanup_saving((char*[]) {lecDir, savePath, resultMsg, NULL}, box);
    return exitCode;
}
