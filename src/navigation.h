#ifndef NAVIGATION_H
#define NAVIGATION_H

#include <stdbool.h>
#include <cjson/cJSON.h>

#include "const.h"

/* STRUCT DEFINITIONS       */

/**
 * Structure that defines a single Lecture for a course.
 */
typedef struct {
    /// The lecture number i.e. one lecture a week give week 7's lecture having
    /// lectureNum = 7
    int      lectureNum;
    bool        downloaded;
} Lecture;

/**
 * Structure that store all necessary information attached to a course.
 * (Tried to debloat the course struct but maybe thats not necessary?)
 */
typedef struct {
    /// Code of course
    char*       courseCode;
    /// Name of the course
    char*       courseName;
    /// Echo360's unique course key needed to get lecture
    char*       courseKey;
    /// Number of recordings available for download in a course
    int      lecCount;
    /// Flag to see if the course is currently studied
    bool        isActive;
} CourseData;

/**
 * Structure that defines a single Course.
 */
typedef struct {
    CourseData* data;
    /// List of lectures available for download in a course
    Lecture*    lectures;
    /// Lecture currently selected by the cursor.
    int      lectureSel;
    /// Used when rendering lecture list in pane given too many lectures for
    /// pane to fit. Identifies the top most lecture number the pane should
    /// render. 1 indexed.
    int      topLecture;
} Course;

/**
 * Describes the cursor of the user within the panes.
 */
typedef struct {
    /// List of available courses
    Course*    courses;
    /// Number of available courses
    int      coursesCount;
    /// Course that cursor is currently highlighting. 
    int      courseSel;
    /// What level the cursor is on, be it Courses or Lectures or potentially
    /// deeper
    int      level;
    /// Top most course being rendered in the pane
    int      topCourse;
} Cursor;

/* FUNCTION DEFINITIONS     */

int get_course_data(CourseData* data, cJSON* course);

int init_cursor(Cursor* cursor, cJSON* json);

bool move_cursor_x(Cursor* cursor, int dx);

bool move_cursor_y(Cursor* cursor, int dy);

void destruct_cursor(Cursor* cursor);

Course* get_course(Cursor* cursor);

int get_lecCount(Cursor* cursor);

char* get_courseKey(Cursor* cursor);

int get_currLec(Cursor* cursor);

int* get_topCourse(Cursor* cursor);

int* get_topLecture(Cursor* cursor);

#endif // NAVIGATION_H
