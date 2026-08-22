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