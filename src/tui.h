#ifndef TUI_H
#define TUI_H

/**
 * Object for a pane on screen.
 */
typedef struct {
    /// The border of the pane
    struct ncplane* frame;
    /// The internals of the pane, overlapping in front of frame.
    struct ncplane* content;
} Pane;

/**
 * The entire display printed to stdout
 */
typedef struct {
    /// The left most pane
    pane parent;
    /// The middle pane, where the cursor is active
    pane current;
    /// The right most pane
    pane preview;
    /// The keybind help pane on bottom screen
    pane help;
} Screen;

/*
 * A keybind label and function. Decorative, currently only used for printing
 * purposes
 */
typedef struct {
    /// The key strokes required to produce function outlines in desc
    char* keys;
    /// The result of pressing keys.
    char* desc;
} keybind;

/**
 * Differentiating each of the parent, current, and preview pane roles attached.
 * Used to write to each pane generically.
 */
typedef enum {
    /// Associated with the parent pane
    ROLE_PARENT,
    /// Associated with the current pane
    ROLE_CURRENT,
    /// Associated with the preview pane
    ROLE_PREVIEW,
    /// Associated with the help pane
    ROLE_HELP
} panerole;

int sigma360_tui(void);

static int watch_lec(char* dir, bool split, char* time);

void dispatch_watch(Cursor* cursor, char* root, bool ss, char* time);

int get_timestamp(struct notcurses* nc, char** timestamp);

void build_download_box(struct notcurses* nc, struct ncplane** box);

#endif // TUI_H
