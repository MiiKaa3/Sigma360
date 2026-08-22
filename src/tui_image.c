#include "tui_image.h"
#include <stdio.h>
#include <string.h>

static struct ncplane *img_plane = NULL;
static char img_current[512];
static unsigned img_rows, img_cols;      /* geometry we last built for */

void sigma360_tui_image_clear(void) {
    if (img_plane) {
        ncplane_destroy(img_plane);
        img_plane = NULL;
    }
    img_current[0] = '\0';
    img_rows = img_cols = 0;
}

int sigma360_tui_image_show(struct ncplane *panel, const char *path) {
    if (!path) {
        sigma360_tui_image_clear();
        return 0;
    }

    unsigned prows, pcols;
    ncplane_dim_yx(panel, &prows, &pcols);
    if (prows < 3 || pcols < 3) {        /* too small to hold anything */
        sigma360_tui_image_clear();
        return 0;
    }
    unsigned rows = prows - 2, cols = pcols - 2;   /* inset past the border */

    if (strcmp(path, img_current) == 0 && rows == img_rows && cols == img_cols) {
        return 0;                        /* same image, same size — done */
    }

    sigma360_tui_image_clear();
    snprintf(img_current, sizeof img_current, "%s", path);
    img_rows = rows;
    img_cols = cols;

    struct ncvisual *ncv = ncvisual_from_file(path);
    if (!ncv) {
        fprintf(stderr, "image: ncvisual_from_file failed: %s\n", path);
        return -1;
    }

    struct ncplane_options nopts = {
        .y = 1, .x = 1,                  /* relative to panel's top-left */
        .rows = rows, .cols = cols,
        .name = "preview-img",
    };
    struct ncplane *n = ncplane_create(panel, &nopts);
    if (!n) {
        ncvisual_destroy(ncv);
        return -1;
    }

    struct ncvisual_options vopts = {
        .n = n,
        .scaling = NCSCALE_SCALE,
        .blitter = NCBLIT_PIXEL,
    };
    if (!ncvisual_blit(ncplane_notcurses(panel), ncv, &vopts)) {
        fprintf(stderr, "image: blit failed for %s\n", path);
        ncvisual_destroy(ncv);
        ncplane_destroy(n);
        return -1;
    }

    ncvisual_destroy(ncv);
    img_plane = n;
    return 0;
}