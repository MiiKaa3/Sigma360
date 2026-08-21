#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <cjson/cJSON.h>

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

int build_tree(char* root, char** courses)
{
    /* char* file; */
    /* read_file("cmds/courses.json", &file); */

    /* cJSON* json = cJSON_parse(file); */
    /* if (json == NULL) { */
    /*     fprintf(stderr, "Bad .json read.\n"); */
    /*     return 11; */
    /* } */
    return 0;
    
}
