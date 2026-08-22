#include "nav.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define NAV_MAX_CHILDREN 4096

// ------------------------------------------- //
// list_t / entry_t management                 //
// ------------------------------------------- //

static void entry_free(entry_t *e);

static void list_free(list_t *l) {
    for (size_t i = 0; i < l->count; i++) {
        entry_free(&l->items[i]);
    }
    free(l->items);
    l->items  = NULL;
    l->count  = 0;
    l->sel    = 0;
    l->top    = 0;
    l->loaded = false;
}
 
static void entry_free(entry_t *e) {
    free(e->label);
    free(e->detail);
    free(e->key);
    e->label = NULL;
    e->detail = NULL;
    e->key = NULL;
    e->node = NULL;
    list_free(&e->children);
}

static int list_alloc(list_t *l, size_t n) {
    l->items = (n > 0) ? calloc(n, sizeof *l->items) : NULL;
    if (n > 0 && l->items == NULL) {
        return -1;
    }
    l->count = 0;
    l->sel   = 0;
    l->top   = 0;
    return 0;
}

static char *dup_or_null(const char *s) {
    return (s != NULL) ? strdup(s) : NULL;
}

// ------------------------------------------- //
// json / data loader                          //
// ------------------------------------------- //

static int load_lectures(nav_t *n, entry_t *course) {
    (void)n;
    if (course->node == NULL) {
        return 0;
    }
 
    cJSON *amt = cJSON_GetObjectItemCaseSensitive(course->node, "lessonCount");
    if (!cJSON_IsNumber(amt) || amt->valueint <= 0) {
        return 0; // course with no lessons; possible?
    }
 
    int total = amt->valueint;
    if (total > NAV_MAX_CHILDREN) {
        total = NAV_MAX_CHILDREN;
    }
    if (list_alloc(&course->children, (size_t)total) != 0) {
        return -1;
    }
 
    for (int i = 1; i <= total; i++) {
        char buf[32];
        snprintf(buf, sizeof buf, "Lecture %d", i);
 
        entry_t *lec = &course->children.items[course->children.count];
        lec->label = strdup(buf);
        if (lec->label == NULL) {
            return -1;
        }
        lec->node = course->node;
        lec->load = NULL;
        course->children.count++;
    }
    return 0;
}
 
static int load_courses(nav_t *n) {
    int size = cJSON_GetArraySize(n->json);
    if (size < 0) {
        size = 0;
    }
    if (list_alloc(&n->root.children, (size_t)size) != 0) {
        return -1;
    }
 
    cJSON *element = NULL;
    cJSON_ArrayForEach(element, n->json) {
        cJSON *code = cJSON_GetObjectItemCaseSensitive(element, "courseCode");
        cJSON *name = cJSON_GetObjectItemCaseSensitive(element, "courseName");
        cJSON *url  = cJSON_GetObjectItemCaseSensitive(element, "url");
 
        if (!cJSON_IsString(code) || code->valuestring == NULL) {
            continue;
        }
        if (n->root.children.count >= (size_t)size) {
            break;
        }
 
        entry_t *e = &n->root.children.items[n->root.children.count];
        e->label = strdup(code->valuestring);
        if (e->label == NULL) {
            return -1;
        }
        e->detail = cJSON_IsString(name) ? dup_or_null(name->valuestring) : NULL;
        e->key    = cJSON_IsString(url)  ? dup_or_null(url->valuestring)  : NULL;
        e->node   = element;
        e->load   = load_lectures;
        n->root.children.count++;
    }
 
    n->root.children.loaded = true;
    return 0;
}

// ------------------------------------------- //
// nav                                         //
// ------------------------------------------- //

int nav_init(nav_t *n, cJSON *json) {
    memset(n, 0, sizeof(*n));
    n->json = json;
    n->depth = 0;
    n->path[0] = &n->root;

    if (load_courses(n) != 0) {
        nav_free(n);
        return -1;
    }

    return 0;
}

void nav_free(nav_t *n) {
    list_free(&n->root.children);
    n->depth = 0;
    n->path[0] = &n->root;
    n->json = NULL;
}

static int ensure_loaded(nav_t *n, entry_t *e) {
    if (e->children.loaded) {
        return 0;
    }

    e->children.loaded = true;
    if (e->load == NULL) {
        return 0;
    }
    return e->load(n, e);
}

list_t *nav_current(nav_t *n) {
    return &n->path[n->depth]->children;
}
 
list_t *nav_parent(nav_t *n) {
    return (n->depth > 0) ? &n->path[n->depth - 1]->children : NULL;
}
 
entry_t *nav_selected(nav_t *n) {
    list_t *l = nav_current(n);
    return (l->count > 0) ? &l->items[l->sel] : NULL;
}
 
list_t *nav_preview(nav_t *n) {
    entry_t *e = nav_selected(n);
    if (e == NULL) {
        return NULL;
    }
    ensure_loaded(n, e);
    return (e->children.count > 0) ? &e->children : NULL;
}
 
void nav_move(nav_t *n, int dy) {
    list_t *l = nav_current(n);
    if (l->count == 0) {
        return;
    }
    long long i = (long long)l->sel + dy;
    if (i < 0) {
        i = (long long)l->count - 1;
    } else if (i >= (long long)l->count) {
        i = 0;
    }
    l->sel = (size_t)i;
}
 
bool nav_descend(nav_t *n) {
    entry_t *e = nav_selected(n);
    if (e == NULL || n->depth + 1 >= NAV_MAX_DEPTH) {
        return false;
    }
    if (ensure_loaded(n, e) != 0 || e->children.count == 0) {
        return false; /* leaf */
    }
    n->path[++n->depth] = e;
    return true;
}
 
bool nav_ascend(nav_t *n) {
    if (n->depth == 0) {
        return false;
    }
    n->depth--;
    return true;
}