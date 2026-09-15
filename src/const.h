#ifndef __ERR_H__
#define __ERR_H__

/*  CONSTANTS           */

#define DEEPEST_LEVEL   1   // Defines the deepest level the cursor can take

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

/*  ERROR MESSAGES      */

extern const char* const badCWD;

extern const char* const watchUsage;

extern const char* const watchTimeUsage; 

extern const char* const badTimestamp;

#endif // __ERR_H__
