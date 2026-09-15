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

int build_tree(char** root)
{
    char* file;
    read_file("./courses.json", &file);
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
            char* lecture = buildLec("Lecture%d", i);
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
    return str;
}

int get_courses_json(char* filename, cJSON** json)
{
    FILE* file = fopen(filename, "r");
    if (!file) {
        // fetch
        pid_t pid = fork();
        if (pid < 0) {
            return BAD;
        }
        if (!pid) {
            execlp("python3", 
                    "python3", fetcher, "--load", "courses.json", NULL);
            // Upon unsuccessful exec
            _exit(BAD);
        }
        int status;
        waitpid(pid, &status, 0);
        if (WIFEXITED(status)) {
            if (WEXITSTATUS(status)) {
                return BAD_FETCH;
            }
        } else {
            return BAD_FETCH;
        }
    } else {
        fclose(file);
    }

    char* contents;
    read_file("./courses.json", &contents);

    *json = cJSON_Parse(contents);
    free(contents);
    if (!*json) {
        cJSON_Delete(*json);
        return BAD_JSON;
    }
    return GOOD;
}

int compare_sem_then_code(const void *a, const void *b)
{
    const cJSON *item_a = *(const cJSON **)a;
    const cJSON *item_b = *(const cJSON **)b;

    const cJSON *yearsem_a 
        = cJSON_GetObjectItemCaseSensitive(item_a, "yearSem");
    const cJSON *yearsem_b 
        = cJSON_GetObjectItemCaseSensitive(item_b, "yearSem");

    const char *str_a = cJSON_IsString(yearsem_a) ? yearsem_a->valuestring : "";
    const char *str_b = cJSON_IsString(yearsem_b) ? yearsem_b->valuestring : "";

    int result = strcmp(str_a, str_b);
    if (result != 0) {
        // reverse it; we want descending order for yearSem
        return -result;
    }

    // Names are equal — fall back to "code" as the tiebreaker
    const cJSON *code_a 
        = cJSON_GetObjectItemCaseSensitive(item_a, "courseCode");
    const cJSON *code_b 
        = cJSON_GetObjectItemCaseSensitive(item_b, "courseCode");

    const char *code_str_a = cJSON_IsString(code_a) ? code_a->valuestring : "";
    const char *code_str_b = cJSON_IsString(code_b) ? code_b->valuestring : "";

    return strcmp(code_str_a, code_str_b);
}

void sort_cjson_array(cJSON *array) {
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
    qsort(items, count, sizeof(cJSON *), compare_sem_then_code); // Sort

    // Place back in a JSON struct
    cJSON *dummy;
    while ((dummy = cJSON_DetachItemFromArray(array, 0)) != NULL) {
        // The function above does everything we want.
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
    if (folder == NULL) {
        // Realistically this should never hit but for safety you know
        return BAD_DIR;
    }
    // readdir goes through each file in a directory until NULL at the end
    while ((d = readdir(folder)) != NULL) {
        if (++n > 3) {
            // We know as fact that the directory will have ., .., and t.jpg
            // Anything else and it will be video/audio files
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

char* build_dir(char* root, char* url, int lectureNum)
{
    char* dir = strdup(root);
    expand_path(&dir);
    dir = buildArgs(dir, url);
    expand_path(&dir);
    char* lecture = buildLec("Lecture%d", lectureNum);
    dir = buildArgs(dir, lecture);
    return dir;
}
