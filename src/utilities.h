#ifndef SIGMA360_UTILITIES_H
#define SIGMA360_UTILITIES_H

#include <stdbool.h>
#include <cjson/cJSON.h>

int findcwd(char** buf);

int read_file(char* dir, char** file);

int build_tree(char** root);

char* buildArgs(char* option, char* var);

cJSON *get_courses_json(char* filename);

void sort_cjson_array(cJSON *array);

bool is_dir_empty(char* dir);

void expand_path(char** path);

char* buildLec(char* option, int num);

#endif // SIGMA360_UTILITIES_H
