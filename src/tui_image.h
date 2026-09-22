#ifndef TUI_IMAGE_H
#define TUI_IMAGE_H

#include <notcurses/notcurses.h>
#include "tui.h"

/**
 * Stores important information any image displayed in a pane. Attached to a 
 * ncplane* structure via the userptr.
 */
typedef struct {
    /// The absolute path of the image being displayed
    char* image;
    /// The notcurses interpretation of the image, for blitting purposes
    struct ncvisual* ncimage;
} imageData;

int preview_image_show(Pane* preview, const char* path);

void preview_image_clear(Pane* preview);

void cleanup_image(Pane* preview);

#endif // TUI_IMAGE_H
