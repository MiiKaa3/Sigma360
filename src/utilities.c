#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <cjson/cJSON.h>
#include <sys/stat.h>
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
    read_file("cmds/courses.json", &file);
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
