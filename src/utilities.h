#ifndef __UTILITIES_H__
#define __UTILITIES_H__

#include <cjson/cJSON.h>

int findcwd(char** buf);

int read_file(char* dir, char** file);

int build_tree(char* root, char** courses);

cJSON *get_json(char* filename);

#endif // __UTILITIES_H__
