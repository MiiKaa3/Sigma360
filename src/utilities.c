/**
 * @file utilities.c
 * @author sammado103, MiiKaa3
 * @brief A collection of useful, non-specific functions used throughout the
 *      program.
 */
#include <stdlib.h>
#include <unistd.h> 
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <dirent.h>

#include <cjson/cJSON.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>

#include "utilities.h"
#include "const.h"

/**
 * Dynamically reads a file into a heap allocated char* buffer. Note that the
 * char* is null terminated.
 * @param dir   String relative path to file for reading.
 * @param file  A pointer to an unitiliased char* to be populated with the file
 *      contents.
 * @returns
 *      BAD_FOPEN   given the file does not exists or fopen fails.
 *      GOOD        upon sucess.
 */
int read_file(char* dir, char** file)
{
    FILE* json = fopen(dir, "r");
    if (json == NULL) {
        return BAD_FOPEN;
    }
    *file = malloc(sizeof(char));
    int size = 0;

    int c;
    while ((c = fgetc(json)) != EOF) {
        *file = realloc(*file, ++size * sizeof(char));
        (*file)[size - 1] = c;
    }
    *file = realloc(*file, ++size * sizeof(char));
    (*file)[size - 1] = '\0';
    fclose(json);
    return GOOD;
}

/**
 * Builds a temporary directory tree in the user's /tmp/ directory. The 
 * resulting folder will be names sigma_XXXXXX/ where each 'X' is replaced with
 * some alphanumeric symbol by mkdtemp(). Each course subfolder will be named
 * with the course's key as defined by Echo360, and lecture folders named
 * 'LectureX' where  'X' is replaced with the lecture number.
 * @param root  A pointer to an uninitialised string to be populated with
 *      /tmp/sigma_XXXXXX
 * @returns
 *      BAD_JSON   given a JSON cannot be generated from courses.json.
 *      GOOD       upon succes.
 */
int build_tree(char** root)
{
    char* file;
    read_file(coursesJSON, &file);
    char template[] = "/tmp/sigma_XXXXXX";
    *root = strdup(mkdtemp(template));

    cJSON* json = cJSON_Parse(file);
    if (json == NULL) {
        return BAD_JSON;
    }
    char* tmp = strdup(*root);
    tmp = realloc(tmp, (strlen(tmp)+strlen("/%s")+1)*sizeof(char));
    strcat(tmp, "/%s");

    cJSON* list = json->child;
    while (list) {
        cJSON* code = cJSON_GetObjectItem(list, "url");
        char* dir = buildArgs(tmp, code->valuestring);
        mkdir(dir, 0777);
        
        dir = realloc(dir, (strlen(dir)+strlen("/%s")+1)*sizeof(char));
        strcat(dir, "/%s");
        cJSON* lessons = cJSON_GetObjectItem(list, "lessonCount");
        
        for (int i = 1; i <= lessons->valueint; i++) {
            char* lecture = buildLec(i);
            char* subdir = buildArgs(dir, lecture);
            mkdir(subdir, 0777);
            free(lecture);
            free(subdir);
        }
        free(dir);
        list = list->next;
    }
    cJSON_Delete(json);
    free(tmp);
    free(file);
    return GOOD;
}

/**
 * Appends 'var' to the end of 'option' given 'option' ends in '%s'. Creates
 * a new heap allocation for the resulting string.
 * <<<REFACTOR REQUIRED>>>
 * @param option String ending in '%s'
 * @param var    String to be appended to option.
 * @returns A new heap allocated string equalling "<option><var>"
 */
char* buildArgs(char* option, char* var) 
{
    int len = snprintf(NULL, 0, option, var);
    char* str = malloc(++len * sizeof(char));
    snprintf(str, len, option, var);
    return str;
}

/**
 * Produces a string "LectureX" where 'X' is replaced with num. Resulting string
 * is heap-allocated and is left to the user to cleanup.
 * @param num Lecture number to append to "Lecture"
 * @returns A heap allocated string equalling "Lecture<num>"
 */
char* buildLec(int num)
{
    int len = snprintf(NULL, 0, "Lecture%d", num);
    char* str = malloc(++len * sizeof(char));
    snprintf(str, len, "Lecture%d", num);
    return str;
}

/**
 * Reads the courses.json into a cJSON struct. Given courses.json does not
 * exist, fetches the courses.json file with get_courses_json() (see fetch.c).
 * @param filename The relative path to courses.json (see const.c)
 * @param json     A pointer to an uninitialised cJSON* struct, to be populated
 *      with this function
 * @returns
 *     BAD          given any syscall fails.
 *     BAD_FETCH    given get_courses_json() fails.
 *     BAD_JSON     given a failure to parse courses.json.
 *     BAD_FOPEN    given read_file() fails.
 *     GOOD         upon success.
 */
int read_courses_json(char* filename, cJSON** json)
{
    int exitCode = GOOD;
    FILE* file = fopen(filename, "r");
    if (!file) {
        if ((exitCode = get_courses_json())) {
            return exitCode;
        }
    } else {
        fclose(file);
    }

    char* contents;
    if ((exitCode = read_file(coursesJSON, &contents))) {
        return exitCode;
    }

    *json = cJSON_Parse(contents);
    free(contents);
    if (!*json) {
        cJSON_Delete(*json);
        return BAD_JSON;
    }
    return exitCode;
}

/**
 * Helper comparitor function used by sort_cjson_array to sort the cJSON* 
 * struct. Compares by Year and Semester first. Bigger year > smaller year,
 * semester 2 > semester 1. Upon same year and same semester, compare 
 * courseCode by strcmp().
 * @param a The first course cJSON* struct to compare
 * @param b The second course cJSON* struct to compare
 * <<<NEED TO VERIFY THIS RETURN>>>
 * @returns
 *      A positive integer given b > a,
 *      A negative integer given a < b,
 *      0 otherwise.
 */
int compare_course(const void* a, const void* b)
{
    const cJSON* course_a = *(const cJSON**) a;
    const cJSON* course_b = *(const cJSON**) b;

    const cJSON* yearSem_a 
        = cJSON_GetObjectItemCaseSensitive(item_a, "yearSem");
    const cJSON* yearSem_b 
        = cJSON_GetObjectItemCaseSensitive(item_b, "yearSem");

    const char* yearSemStr_a = cJSON_IsString(yearsem_a) ? 
        yearSem_a->valuestring : "";
    const char* yearSemStr_b = cJSON_IsString(yearsem_b) ? 
        yearSem_b->valuestring : "";

    int comp = strcmp(yearSemStr_a, yearSemStr_b);
    if (comp != 0) {
        return -comp; // Negative for descending order.
    }

    // Same year + sem => sort by strcmp() on courseCode 
    const cJSON* courseCode_a 
        = cJSON_GetObjectItemCaseSensitive(item_a, "courseCode");
    const cJSON* courseCode_b 
        = cJSON_GetObjectItemCaseSensitive(item_b, "courseCode");

    const char* courseCodeStr_a = cJSON_IsString(courseCode_a) ? 
        courseCode_a->valuestring : "";
    const char* courseCodeStr_b = cJSON_IsString(courseCode_b) ? 
        courseCode_b->valuestring : "";
    return strcmp(courseCodeStr_a, courseCodeStr_b);
}

/**
 * Sorts the cJSON array according the the compare_course() comparator. 
 * @param array The cJSON array to be sorted.
 */
void sort_cjson_array(cJSON* array) 
{
    int count = cJSON_GetArraySize(array);
    // If we have either 0 or 1 item, its trivially sorted
    if (count < 2) {
        return;
    }

    // We need the JSON items in an array to sort
    cJSON** items = malloc(count * sizeof(cJSON*));
    for (int i = 0; i < count; i++) {
        items[i] = cJSON_GetArrayItem(array, i);
    }
    qsort(items, count, sizeof(cJSON*), compare_course); // Sort

    // Place back in a JSON struct
    while (cJSON_DetachItemFromArray(array, 0)) {
        // The above conditional removes each Item from the cJSON, to be put
        // back in the correct order below
    }

    for (int i = 0; i < count; i++) {
        cJSON_AddItemToArray(array, items[i]);
    }
    free(items);
}

/**
 * Checks whether a lecture has been downloaded in a /tmp/sigma_XXXXXX lecture
 * folder. Checks to see if "v1.mp4", "v2.mp4", and "audio.mp4" all exist
 * in the provided directory.
 * @param dir The /tmp/sigma_XXXXXX directory to be checked
 * @returns
 *      BAD_DIR     if directory given does not exist.
 *      1           if v1.mp4, v2.mp4, and audio.mp4 exists in dir.
 *      0           otherwise.
 */
int is_lec_downloaded(char* dir)
{
    int n = 0;
    struct dirent* d;
    DIR* folder = opendir(dir);
    if (folder == NULL) {
        // Realistically this should never hit but for safety you know
        return BAD_DIR;
    }
    // readdir goes through each file in a directory until NULL at the end
    while ((d = readdir(folder)) != NULL) {
        if (!strcmp(d->d_name, "v1.mp4") || !strcmp(d->d_name, "v2.mp4")
                && strcmp(d->d_name,"audio.mp4")) {
            n++;
        }
    }
    closedir(folder);
    return n == 3 ? 1 : 0;
}

/**
 * Converts a string into a '/%s' terminating string. Essentialy converts any
 * string into a directory equivalent string. Requires the provided string to
 * be heap-allocated.
 * @param path A pointer to a HEAP-ALLOCATED char* to be expanded into a dir.
 */
void expand_path(char** path)
{
    *path = realloc(*path, (strlen(*path)+strlen("/%s")+1)*sizeof(char));
    strcat(*path, "/%s");
}

/**
 * Constructs a string representing the /tmp/sigma_XXXXXX directory for a given
 * course's lectures. Creates a heap-allocated char* with such a directory.
 * @param root   The tmp root directory in the form '/tmp/sigma360'
 * @param url    The unique course id provided by Echo360. Can be found in 
 *      the Course's CourseData struct.
 * @param lecNum The desired lecture number to build the directory of.
 * @return A heap-allocated char* of the form
 *      "/tmp/sigma_XXXXXX/<url>/Lecture<lecNum>"
 */
char* build_dir(char* root, char* url, int lecNum)
{
    char* dir = strdup(root);
    expand_path(&dir);
    char* course = buildArgs(dir, url);
    free(dir)
    expand_path(&course);
    char* lecName = buildLec(lecNum);
    char* lecture = buildArgs(course, lecName);
    free(course);
    return lecture;
}
