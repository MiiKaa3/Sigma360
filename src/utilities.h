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

char* build_args(char* option, char* var);

void sort_cjson_array(cJSON *array);

int is_lec_downloaded(char* dir);

void expand_path(char** path);

char* build_lec(int num);

char* build_dir(char* root, char* url, int lectureNum);

int read_courses_json(char* filename, cJSON** json);

int build_popup(struct notcurses* nc, struct ncplane** box, 
        int rows, int cols);

int build_reader(struct ncplane* box, struct ncreader** reader,
        struct ncplane_options options);

int read_popup_input(struct notcurses* nc, struct ncreader* reader,
    char** result);

void block_for_input(struct notcurses* nc);

int build_dump_file(char** dumpFile);

#endif // UTILITIES_H
