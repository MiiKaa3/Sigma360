#ifndef SAVING_TMP_H
#define SAVING_TMP_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

#include <notcurses/nckeys.h>
#include <notcurses/notcurses.h>

#include "navigation.h"
#include "utilities.h"
#include "const.h"

int save_lecture(struct notcurses* nc, Cursor* cursor, const char* root);

#endif // SAVING_TMP_H
