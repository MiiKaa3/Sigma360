/**
 * @file utilities.h
 * @author sammado103, MiiKaa3
 * @brief Header for utilities.c. See utilities.c for more info.
 */
#ifndef UTILITIES_H
#define UTILITIES_H

#include <stdbool.h>
#include <cjson/cJSON.h>
#include <notcurses/notcurses.h>

int read_file(char* dir, char** file);

int build_tree(char** root);

char* buildArgs(char* option, char* var);

void sort_cjson_array(cJSON *array);

int is_lec_downloaded(char* dir);

void expand_path(char** path);

char* buildLec(int num);

char* build_dir(char* root, char* url, int lectureNum);

int read_courses_json(char* filename, cJSON** json);

int build_popup(struct notcurses* nc, struct notcurses** box, 
        int rows, int cols)

int build_reader(struct ncplane* box, struct ncreader** reader,
        struct ncplane_options options);

#endif // UTILITIES_H
