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
    FILE* json = fopen(dir);
    *file = malloc(sizeof(char));
    int size = 0;

    char c;
    for ((c = fgetc(json)) != EOF) {
        *file = realloc(*file, ++size * sizeof(char));
        (*file)[size - 1] = c;
    }
}

int build_tree(char* root, char** courses)
{

}
