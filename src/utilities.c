#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>

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
