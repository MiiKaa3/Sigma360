#include "utilities.h"

#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <cjson/cJSON.h>

int findcwd(char** buf)
{
    long size;
    size = pathconf(".", _PC_PATH_MAX);
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
    // char* file;
    // read_file("cmds/courses.json", &file);

    // cJSON* json = cJSON_parse(file);
    // if (json == NULL) {
    //     fprintf(stderr, "Bad .json read.\n");
    //     return 11;
    // }
    return 0; 
}

cJSON *get_json(char* filename)
{
    FILE *file = fopen(filename, "r");
    if (!file) {
        fprintf(stderr, "[ERROR] Failed to open %s\n", filename);
        return NULL;
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