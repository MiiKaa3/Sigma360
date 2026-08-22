#ifndef SIGMA360_UTILITIES_H
#define SIGMA360_UTILITIES_H

#include <cjson/cJSON.h>

int findcwd(char** buf);

int read_file(char* dir, char** file);

int build_tree(char* root, char** courses);

char* buildArgs(char* option, char* var);

cJSON *get_json(char* filename);

void sort_cjson_array(cJSON *array);

#endif // SIGMA360_UTILITIES_H
