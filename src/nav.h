#ifndef SIGMA360_NAV_H
#define SIGMA360_NAV_H

#include <stdbool.h>
#include <stddef.h>

#include <cjson/cJSON.h>

#define NAV_MAX_DEPTH 8

typedef struct entry entry_t;
typedef struct nav   nav_t;

typedef int (*nav_loader_fn)(nav_t *n, entry_t *e);

typedef struct {
    entry_t *items;
    size_t   count;
    size_t   sel;
    size_t   top;
    bool     loaded;
} list_t;

struct entry {
    char         *label;
    char         *url;
    char         *detail;
    char         *key;
    cJSON        *node;
    list_t        children;
    nav_loader_fn load;
};

struct nav {
    entry_t  root;
    entry_t *path[NAV_MAX_DEPTH];
    int      depth;
    cJSON   *json;
};

int  nav_init(nav_t *n, cJSON *json);
void nav_free(nav_t *n);

list_t  *nav_parent(nav_t *n);
list_t  *nav_current(nav_t *n);
list_t  *nav_preview(nav_t *n);

entry_t *nav_selected(nav_t *n);

void nav_move(nav_t *n, int dy);
bool nav_descend(nav_t *n);
bool nav_ascend(nav_t *n);

#endif // SIGMA360_NAV_H
