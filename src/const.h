/**
 * @file const.h
 * @authour sammado103
 * @brief Header for const.c. Similarly stores macros for error messages and 
 * other number constants like rgb values.
 */
#ifndef CONST_H
#define CONST_H

/*  CONSTANTS           */

#define DEEPEST_LEVEL     1  // Defines the deepest level the cursor can take
#define DETAILS_MIN_COLS  10 // The minimum number of columns for each pane
#define HELP_ROWS         3  // The number of rows the HELP pane takes up
#define MIN_COLS          3  // The minimum number of columns a pane can have
#define MIN_ROWS          3  // The minimum number of rows a pane can have

#define COL_TEXT_DEF      0xf0f0f0
#define COL_BORDER_DIM    0x6272a4u
#define COL_BORDER_ACTIVE 0xbd93f9u
#define COL_SEL_BG_ACTIVE 0x2f7482u
#define COL_SEL_BG_IDLE   0x44475au
#define COL_SEL_FG        0xf8f8f2u
#define COL_MODAL_BG      0x282a36u
#define COL_HELP_KEY      0xbd93f9u
#define COL_HELP_DESC     0x91bbffu
#define COL_ACTIVE_COURSE 0xffd866

/*  STRING CONSTANTS    */

extern const char* const version;

extern const char* const defaultImage;
extern char* const coursesJSON;
extern const char* const fetcher;
extern char* const watch;
extern const char* const girlScout;

/*  ERROR CODES         */

#define BAD            -1
#define GOOD            0
#define BAD_CWD         1
#define BAD_FETCH       2
#define BAD_WATCH_PARSE 3
#define BAD_TIMESTAMP   4
#define BAD_FOPEN       5
#define BAD_JSON        6
#define BAD_DIR         7
#define BAD_CURSOR      8
#define BAD_PANES       9
#define BAD_CMD_EXEC    10
#define BAD_COOKIES     12
#define BAD_LEC_GET     13
#define BAD_THUMB       14
#define BAD_USAGE       15
#define BAD_PANE        16
#define BAD_SIZE        17
#define BAD_IMAGE       18

/*  ERROR MESSAGES      */

extern const char* const badCWD;
extern const char* const watchUsage;
extern const char* const watchTimeUsage; 
extern const char* const badTimestamp;
extern const char* const mainUsage;

extern const char* const mainHelp;
#endif // CONST_H
