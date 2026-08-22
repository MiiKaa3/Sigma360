#ifndef SIGMA360_TUI_IMAGE_H
#define SIGMA360_TUI_IMAGE_H

#include <notcurses/notcurses.h>

int sigma360_tui_image_show(struct ncplane *panel, const char *path);
void sigma360_tui_image_clear(void);

#endif