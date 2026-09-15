#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <cjson/cJSON.h>

#include "navigation.h"
#include "const.h"

/**
 * Initialises the cursor struct used in tui.c to maintain user's highlighted
 * courses and lectures. Initialisation involves generating course-lecture
 * tree and adequately populating those structure.
 * @param cursor A pointer to an empty Cursor struct initialised in the 
 * tui.c call to this function
 * @param json A cJSON structure defining the courses.json file read from
 * Echo360.
 * @returns (see src/const.h for precise macro definitions)
 *      BAD_JSON    upon a bad read of the courses.json file. Typically not
 *          this functions fault. This function just check for what is expected.
 *      GOOD        upon successful inititaion 
 */
int init_cursor(Cursor* cursor, cJSON* json)
{
    int exitCode = GOOD;
    cursor->courseSel = 0;
    cursor->level = 0;
    cursor->topCourse = 0;
    cursor->coursesCount = cJSON_GetArraySize(json);

    cursor->courses = malloc(cursor->coursesCount * sizeof(Course));

    cJSON* courseJSON;
    int i = 0;
    cJSON_ArrayForEach(courseJSON, json) {
        Course* course = &(cursor->courses[i]);

        course->lectureSel = 0;
        course->topLecture = 0;
        course->data = malloc(sizeof(CourseData));

        if ((exitCode = get_course_data(course->data, courseJSON))) {
            free(cursor->courses);
            free(course->data);
            return exitCode;
        }

        course->lectures = malloc(course->data->lecCount * sizeof(Lecture));
        for (int j = 0; j < course->data->lecCount; j++) {
            course->lectures[j].lectureNum = j + 1;
            course->lectures[j].downloaded = false;
        }
        i++;
    }
    return exitCode;
}

int get_course_data(CourseData* data, cJSON* course)
{
    cJSON* code = cJSON_GetObjectItemCaseSensitive(course, "courseCode"); 
    cJSON* name = cJSON_GetObjectItemCaseSensitive(course, "courseName");
    // Unique course key described by Echo360
    cJSON* key = cJSON_GetObjectItemCaseSensitive(course, "url");
    cJSON* lectureCount 
        = cJSON_GetObjectItemCaseSensitive(course, "lessonCount");
    cJSON* isActive = cJSON_GetObjectItemCaseSensitive(course, "isActive");

    if (!cJSON_IsString(code) || !code->valuestring) { return BAD_JSON; }
    data->courseCode = strdup(code->valuestring);

    if (!cJSON_IsString(name) || !name->valuestring) { return BAD_JSON; }
    data->courseName = strdup(name->valuestring);

    if (!cJSON_IsString(key) || !key->valuestring) { return BAD_JSON; }
    data->courseKey = strdup(key->valuestring);

    if (!cJSON_IsNumber(lectureCount) ) { return BAD_JSON; }
    data->lecCount = lectureCount->valueint;

    data->isActive = cJSON_IsTrue(isActive) ? true : false;
    return GOOD;
}

bool move_cursor_y(Cursor* cursor, int dy)
{
    if (cursor->level + dy > DEEPEST_LEVEL) {
        cursor->level = DEEPEST_LEVEL;
    } else if (cursor->level + dy < 0) {
        cursor->level = 0;
    } else {
        cursor->level += dy;
    }
    return true;
}

bool move_cursor_x(Cursor* cursor, int dx)
{
    switch (cursor->level) {
        case 0: {
            int coursesCnt = cursor->coursesCount;
            cursor->courseSel = ((cursor->courseSel + dx) 
                    % coursesCnt + coursesCnt) % coursesCnt;
            break;
        }
        case 1: {
            int course = cursor->courseSel;
            int lecCnt = cursor->courses[course].data->lecCount;
            cursor->courses[course].lectureSel 
                = ((cursor->courses[course].lectureSel + dx)
                        % lecCnt + lecCnt) % lecCnt;
            break;
        }
        default:
            return false;
            break;
    }
    return false;
}

// Some helper functions to get things we want

Course* get_course(Cursor* cursor)
{
    return &(cursor->courses[cursor->courseSel]);
}

int get_lecCount(Cursor* cursor)
{
    Course* course = get_course(cursor);
    return course->data->lecCount;
}

char* get_courseKey(Cursor* cursor)
{
    Course* course = get_course(cursor);
    return course->data->courseKey;
}

int get_currLec(Cursor* cursor)
{
    Course* course = get_course(cursor);
    return course->lectureSel;
}

int* get_topCourse(Cursor* cursor)
{
    return &(cursor->topCourse);
}

int* get_topLecture(Cursor* cursor)
{
    Course* course = get_course(cursor);
    return &(course->topLecture);
}

// Cleanup

void destruct_cursor(Cursor* cursor)
{
    for (int i = 0; i < cursor->coursesCount; i++) {
        free(cursor->courses[i].lectures);
        free(cursor->courses[i].data->courseCode);
        free(cursor->courses[i].data->courseName);
        free(cursor->courses[i].data->courseKey);
        free(cursor->courses[i].data);
    }
    free(cursor->courses);
}
