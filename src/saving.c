#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <notcurses/nckeys.h>
#include <notcurses/notcurses.h>

#include "navigation.h"
#include "utilities.h"
#include "const.h"

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
