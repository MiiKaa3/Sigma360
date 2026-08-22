#include <stdlib.h>
#include <unistd.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <dirent.h>
#include <math.h>
#include <ctype.h>
#include <cjson/cJSON.h>
#include <sys/stat.h>
#include <sys/types.h>
#include "utilities.h"

int findcwd(char** buf)
{
    size_t size;
    size = (size_t) pathconf(".", _PC_PATH_MAX);
    if (size == -1) {
        fprintf(stderr, "CHECK YOUR FILE PATH LIMITS!!!!!");
        return -1;
    }
    *buf = malloc(size * sizeof(char));
    getcwd(*buf, size);
    return 0;
}

int read_file(char* dir, char** file)
{
    FILE* json = fopen(dir, "r");
    if (json == NULL) {
        fprintf(stderr, 
                "Failed opening .json. Check directory or existence.\n");
        return 10;
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
    return 0;
}

int build_tree(char** root)
{
    char* file;
    char* jDir;
    if (findcwd(&jDir)) {
        return -1;
    }
    strcat(jDir, "/courses.json");
    read_file(jDir, &file);
    char template[] = "/tmp/sigma_XXXXXX";
    *root = mkdtemp(template);

    cJSON* json = cJSON_Parse(file);
    if (json == NULL) {
        fprintf(stderr, "Bad .json read.\n");
        return 11;
    }
    char* tmp = strdup(*root);
    tmp = realloc(tmp, (strlen(tmp)+strlen("/%s")+1)*sizeof(char));
    strcat(tmp, "/%s");

    cJSON* list = json->child;
    while (list) {
        cJSON* code = cJSON_GetObjectItem(list, "courseCode");
        char* dir = buildArgs(tmp, code->valuestring);
        mkdir(dir, 0777);
        
        dir = realloc(dir, (strlen(dir)+strlen("/%s")+1)*sizeof(char));
        strcat(dir, "/%s");
        cJSON* lessons = cJSON_GetObjectItem(list, "lessonCount");
        
        for (int i = 1; i <= lessons->valueint; i++) {
            int len = snprintf(NULL, 0, "Lecture%d", i);
            char* lecture = malloc(++len * sizeof(char));
            snprintf(lecture, len, "Lecture%d", i);
            char* subdir = buildArgs(dir, lecture);
            mkdir(subdir, 0777);
            free(lecture);
            free(subdir);
        }
        free(dir);
        list = list->next;
    }

    return 0;
}

char* buildArgs(char* option, char* var) 
{
    int len = snprintf(NULL, 0, option, var);
    char* str = malloc(++len * sizeof(char));
    snprintf(str, len, option, var);
    return str;
}

char* buildLec(char* option, int num)
{
    int len = snprintf(NULL, 0, option, num);
    char* str = malloc(++len * sizeof(char));
    snprintf(str, len, option, num);
    free(option);
    return str;
}

cJSON *get_courses_json(char* filename)
{
    FILE *file = fopen(filename, "r");
    if (!file) {
        // fetch
        if (system("( cd ./src/cmds ; python3 fetcher.py --load )") != 0) {
            fprintf(stderr, "[ERROR] Failed to fetch courses.json\n");
        }
        file = fopen(filename, "r");
        if (!file) {
            fprintf(stderr, "[ERROR] Failed to open %s\n", filename);
            return NULL;
        }
    }

    fseek(file, 0, SEEK_END);
    long file_size = ftell(file);
    fseek(file, 0, SEEK_SET);

    char *file_content = malloc(file_size + 1);
    if (!file_content) {
        fprintf(stderr, "[ERROR] Memory allocation failed\n");
        fclose(file);
        return NULL;
    }

    fread(file_content, 1, file_size, file);
    file_content[file_size] = '\0';
    fclose(file);

    cJSON *json_data = cJSON_Parse(file_content);
    free(file_content);
    if (!json_data) {
        fprintf(stderr, "[ERROR] Failed to parse JSON\n");
        return NULL;
    }

    const char *error_ptr = cJSON_GetErrorPtr();
    if (error_ptr != NULL) {
        fprintf(stderr, "[ERROR] JSON Error before: %s\n", error_ptr);
    }

    return json_data;
}

int compare_sem_then_code(const void *a, const void *b) {
    const cJSON *item_a = *(const cJSON **)a;
    const cJSON *item_b = *(const cJSON **)b;

    const cJSON *yearsem_a = cJSON_GetObjectItemCaseSensitive(item_a, "yearSem");
    const cJSON *yearsem_b = cJSON_GetObjectItemCaseSensitive(item_b, "yearSem");

    const char *str_a = cJSON_IsString(yearsem_a) ? yearsem_a->valuestring : "";
    const char *str_b = cJSON_IsString(yearsem_b) ? yearsem_b->valuestring : "";

    int result = strcmp(str_a, str_b);
    if (result != 0) {
        // reverse it; we want descending order for yearSem
        return -result;
    }

    // Names are equal — fall back to "code" as the tiebreaker
    const cJSON *code_a = cJSON_GetObjectItemCaseSensitive(item_a, "courseCode");
    const cJSON *code_b = cJSON_GetObjectItemCaseSensitive(item_b, "courseCode");

    const char *code_str_a = cJSON_IsString(code_a) ? code_a->valuestring : "";
    const char *code_str_b = cJSON_IsString(code_b) ? code_b->valuestring : "";

    return strcmp(code_str_a, code_str_b);
}

void sort_cjson_array(cJSON *array) {
    int count = cJSON_GetArraySize(array);
    if (count < 2) return;

    // Step 1: pull pointers out into a plain C array
    cJSON **items = malloc(count * sizeof(cJSON *));
    for (int i = 0; i < count; i++) {
        items[i] = cJSON_GetArrayItem(array, i);
    }

    // Step 2: sort the plain array
    qsort(items, count, sizeof(cJSON *), compare_sem_then_code);

    // Step 3: detach all items from the original array (without freeing them)
    // then re-add them in sorted order
    cJSON *dummy;
    while ((dummy = cJSON_DetachItemFromArray(array, 0)) != NULL) {
        // just draining the array; items[] still holds valid pointers
    }

    for (int i = 0; i < count; i++) {
        cJSON_AddItemToArray(array, items[i]);
    }

    free(items);
}

bool is_dir_empty(char* dir)
{
    int n = 0;
    struct dirent* d;
    DIR* folder = opendir(dir);
    // Need some better way of handling bad directory. Just assume its right
    // for the minute
    if (folder == NULL) {
        fprintf(stderr, "Directory does not exist\n");
        return true;
    }
    while ((d = readdir(folder)) != NULL) {
        if (++n > 3) {
            // We necessarily get . and .., anything else and the directory
            // is non-empty, so return false;
            closedir(folder);
            return false;
        }
    }
    closedir(folder);
    return true;
}

void expand_path(char** path)
{
    *path = realloc(*path, (strlen(*path)+strlen("/%s")+1)*sizeof(char));
    strcat(*path, "/%s");
}
