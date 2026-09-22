/**
 * @file navigation.c
 * @author sammado103 (refactored from MiiKaa3's work)
 * @brief Describes the cursor and its position throughout the program.
 */
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

/**
 * Reads a course JSON entry and checks for and populated the appropriate data
 * fields in a course's CourseData struct. No entry should be missing or
 * inappropriate in any way, this would be the fetcher function failing in some
 * capacity. This checks nonetheless.
 * @param data    A pointer to the CourseData struct attached to a Course.
 * @param course  The JSON entry for a given Course. Only fields that are used
 *          throughout the program life are stored.
 * @returns
 *      BAD_JSON    given any JSON field is not/inappropriately populated.
 *      GOOD        upon success.
 */
static int get_course_data(CourseData* data, cJSON* course)
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

/**
 * Moves the cursor left and right between panes, given a dy value.
 * @param cursor A pointer to the program's cursor instance.
 * @param dy     Number of panes to move over. +dy is moving right, -dy is 
 *      moving left
 * @returns true always. Later functionality may implement a false condition.
 */
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

/**
 * Moves the cursor up and down within a pane, given a dx value.
 * @param cursor A pointer to the program's cursor instance.
 * @param dx     Number of entries to move up and down within a pane. +dx is
 *      moving up, -dx is moving down. Wraps cursor around, so going past the
 *      bottom of the list being walk talks you to the front and vice versa.
 * @returns true always. Can return false given DEEPEST_LEVEL < cursor->level
 *      and cursor->level < LOWEST_LEVEL, but current implementation does not
 *      allow this.
 */
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
    return true;
}

/* HELPER FUNCTIONS     */

/**
 * Gets the current course selected by the cursor.
 * @param cursor A pointer to the program's cursor instance.
 * @returns A pointer to the currently selected course Course struct. 
 */
Course* get_course(Cursor* cursor)
{
    return &(cursor->courses[cursor->courseSel]);
}

/**
 * Gets the number of lectures attached to the current course.
 * @param cursor A pointer to the program's cursor instance.
 * @returns The number of lectures attached to the current course.
 */
int get_lecCount(Cursor* cursor)
{
    Course* course = get_course(cursor);
    return course->data->lecCount;
}

/**
 * Gets the course key for the current course. Course key is defined by ECHO360.
 * @param cursor A pointer to the program's cursor instance.
 * @returns The current course's key.
 */
char* get_courseKey(Cursor* cursor)
{
    Course* course = get_course(cursor);
    return course->data->courseKey;
}

/**
 * Gets the current lecture of the current course being selected by the cursor.
 * @param cursor A pointer to the program's cursor instance.
 * @returns The current lecture being selected within the current course.
 */
int get_currLec(Cursor* cursor)
{
    Course* course = get_course(cursor);
    return course->lectureSel;
}

/**
 * Gets the top most course to be rendered in the pane. Typically will be 0,
 * unless user has taken a lot of courses. 
 * @param cursor A pointer to the program's cursor instance.
 * @returns A pointer to the top most course's index.
 */
int* get_topCourse(Cursor* cursor)
{
    return &(cursor->topCourse);
}

/**
 * Gets the top most lecture of a course to be rendered in the pane. Typically 
 * will be 0 unless course has a lot of lectures.
 * @param cursor A pointer to the program's cursor instance.
 * @returns A pointer to the top most lecture's index.
 */
int* get_topLecture(Cursor* cursor)
{
    Course* course = get_course(cursor);
    return &(course->topLecture);
}

/*  CLEANUP             */

/**
 * Memory cleanup routine for the cursor. The Courses array, each Lecture array,
 * and each CourseData array are heap-allocated. Any string data within
 * CourseData is also heap-allocated.
 * @param cursor A pointer to the program's cursor instance.
 */
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
