#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/wait.h>

#include <notcurses/nckeys.h>
#include <notcurses/notcurses.h>

#include "navigation.h"
#include "utilities.h"
#include "const.h"
#include "fetch.h"

#define ESCAPE     -1

static void cleanup_saving(char* strs[], struct ncplane* box)
{
    while (strs[0]) {
        free(strs[0]);
        strs++;
    }
    ncplane_destroy(box);
}


static int get_save_path(struct notcurses* nc, struct ncplane* box,
        char** path)
{
    int exitCode = GOOD;

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
    return exitCode;
}

static int validate_msg(struct ncplane* box, char* msg, char** newMsg)
{
    // Note - 2 as we have a border.
    unsigned height = ncplane_dim_y(box) - 2;
    unsigned width = ncplane_dim_x(box) - 2;

    int size = 0;
    *newMsg = malloc(sizeof(char));
    for (int i = 0; i < (int) strlen(msg) && i < (int) (height * width); i++) {
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
    if (*box != NULL) {
        ncplane_destroy(*box);
    }
    if ((exitCode = build_popup(nc, box, SAVE_BOX_H, SAVE_BOX_W))) {
        return exitCode;
    }
    ncplane_set_scrolling(*box, true);

    char* newMsg;
    ncplane_putstr_yx(*box, 1, 2, msg);
    notcurses_render(nc);
    return exitCode;
}

static void block_for_input(struct notcurses* nc)
{
    struct ncinput input;
    while (true) {
        uint32_t id = notcurses_get_blocking(nc, &input);
        if (id == (uint32_t) -1 || input.evtype != NCTYPE_RELEASE) {
            break;
        }
    }
}

static int stitch_lecture(char* tmpDir, char* destination, char* saveName)
{
    const char* home = getenv("HOME");
    int len = snprintf(NULL, 0, "%s/v1.mp4", tmpDir);
    char* video1 = malloc(++len * sizeof(char));
    snprintf(video1, len, "%s/v1.mp4", tmpDir);

    len = snprintf(NULL, 0, "%s/v2.mp4", tmpDir);
    char* video2 = malloc(++len * sizeof(char));
    snprintf(video2, len, "%s/v2.mp4", tmpDir);

    len = snprintf(NULL, 0, "%s/audio.mp4", tmpDir);
    char* audio = malloc(++len * sizeof(char));
    snprintf(audio, len, "%s/audio.mp4", tmpDir);

    len = snprintf(NULL, 0, "%s/%s/%s_1.mp4", home, destination, saveName);
    char* leftVid = malloc(++len * sizeof(char));
    snprintf(leftVid, len, "%s/%s/%s_1.mp4", home, destination, saveName);

    len = snprintf(NULL, 0, "%s/%s/%s_2.mp4", home, destination, saveName);
    char* rightVid = malloc(++len * sizeof(char));
    snprintf(rightVid, len, "%s/%s/%s_2.mp4", home, destination, saveName);

    pid_t left = fork();
    pid_t right = -1;
    if (left) {
        right = fork();
    }
    if (left < 0 || (right < 0 && left)) {
        return BAD;
    }
    if (!left || !right) {
        int fd = open("/dev/null", O_RDWR);
        dup2(fd, STDOUT_FILENO);
        dup2(fd, STDERR_FILENO);
        close(fd);
        char* args[] = {
            "ffmpeg", "-nostdin", "-y",
            "-i", (left == 0) ? video1 : video2,
            "-i", audio,
            "-map", "0:v:0",
            "-map", "1:a:0",
            "-c", "copy",
            "-shortest",
            (left == 0) ? leftVid : rightVid,
            NULL
        };
        execvp("ffmpeg", args);
        _exit(BAD);
    }
    int leftStatus;
    int rightStatus;
    waitpid(left, &leftStatus, 0);
    waitpid(right, &rightStatus, 0);
    if (WIFEXITED(leftStatus) || WIFEXITED(rightStatus)) {
        if (WEXITSTATUS(leftStatus) || WEXITSTATUS(rightStatus)) {
            return BAD_SAVE;
        }
    } else {
        return BAD_SAVE;
    }
    free(video1); free(video2); free(audio); 
    free(leftVid); free(rightVid);
    return GOOD;
}

int save_lecture(struct notcurses* nc, Cursor* cursor, char* root)
{
    int exitCode = GOOD;
    char* savePath;

    struct ncplane* box = NULL;
    if ((exitCode = draw_savebox(nc, &box, 
                    "Enter save path (relative to home dir):"))) {
        return exitCode;
    }

    if ((exitCode = get_save_path(nc, box, &savePath))) {
        ncplane_destroy(box);
        return exitCode;
    }
    char* key = get_courseKey(cursor);
    int lecNum = get_currLec(cursor) + 1;
    char* lecDir = build_dir(root, key, lecNum);
    
    if (!is_lec_downloaded(lecDir)) {
        draw_savebox(nc, &box, "Downloading lecture.");
        if ((exitCode = get_lecture(lecDir))) {
            // NEED SOME ERROR HANDLING HERE WHEN FAILS
            cleanup_saving((char*[]) {lecDir, savePath, NULL}, box);
            return exitCode;
        }
    }

    char* code = get_code(cursor);
    int len = snprintf(NULL, 0, "%s_LEC%d", code, lecNum);
    char* saveName = malloc(++len * sizeof(char));
    snprintf(saveName, len, "%s_LEC%d", code, lecNum);

    if ((exitCode = stitch_lecture(lecDir, savePath, saveName))) {
        draw_savebox(nc, &box, "Could not save lecture to:");
    } else {
        draw_savebox(nc, &box, "Saved lecture to:");
    }
    fprintf(stderr, "%s\n", savePath);
    ncplane_putstr_yx(box, 2, 2, savePath);
    notcurses_render(nc);
    block_for_input(nc);

    cleanup_saving((char*[]) {lecDir, savePath, NULL}, box);
    return exitCode;
}
