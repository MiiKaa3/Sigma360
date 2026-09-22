/**
 * @file main.c
 * @author MiiKaa3
 * @brief The entry into the program. 
 */
#include <stdio.h>
#include <string.h>

#include "tui.h"
#include "const.h"    

void version_msg();
void usage_msg();

int main(int argc, char **argv) 
{
    int exitCode = GOOD;
    if (argc > 1) {
        if (strcmp(argv[1], "--version") == 0) {
            version_msg();
        } else if (strcmp(argv[1], "--help") == 0) {
            usage_msg();
        } else {
            fprintf(stder, MAIN_USAGE);
            return BAD_USAGE;
    } else {
        exitCode = sigma360_tui();
    }
    return exitCode;
}

/**
 * Prints to stdout the current version number of Sigma360.
 */
void version_msg()
{
    printf(mainUsage);
}

/**
 * Prints to stdout the help message.
 */
void usage_msg()
{
    printf(mainHelp);
}
