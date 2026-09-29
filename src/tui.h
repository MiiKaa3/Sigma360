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
    Pane parent;
    /// The middle pane, where the cursor is active
    Pane current;
    /// The right most pane
    Pane preview;
    /// The keybind help pane on bottom screen
    Pane help;
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

int tui(void);

#endif // TUI_H
