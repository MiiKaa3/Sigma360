/**
 * @file utilities.h
 * @author sammado103, MiiKaa3
 * @brief Header for utilities.c. See utilities.c for more info.
 */
#ifndef UTILITIES_H
#define UTILITIES_H

#include <stdbool.h>
#include <cjson/cJSON.h>

int read_file(char* dir, char** file);

int build_tree(char** root);

char* buildArgs(char* option, char* var);

void sort_cjson_array(cJSON *array);

int is_lec_downloaded(char* dir);

void expand_path(char** path);

char* buildLec(int num);

char* build_dir(char* root, char* url, int lectureNum);

int read_courses_json(char* filename, cJSON** json);

#endif // UTILITIES_H
