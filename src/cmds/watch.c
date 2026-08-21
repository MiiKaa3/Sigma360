#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <ctype.h>
#include "../utilities.h"

/*  STRUCTS  */

typedef struct {
    bool splitScreen; // -s
    char* startTime; // -t [timestamp as HH:MM:SS]
} Options;

typedef struct {
    char* course;
    char* lecture;
    Options options;
} Parameters;

/*  MACROS  */

#define GOOD      0
#define BAD_USAGE 1
#define BAD_TIME  2

/*  STRING CONSTANTS  */

const char* const usage = 
    "Usage: watch [-s, -t [timestamp]] [-l [lecture]]\n"
    "Note that the lecture to watch must be the final argument.\n";

const char* const timeUsage = 
    "Start time argument is to be given as HH:MM:SS\n";

const char* const help =
    "play selected lecture recording. Uses mpv as video player. If this is not "
    "installed, this will command will fail.\n"
    "Usage: watch [-s, -t [timestamp]] [-l [lecture]]\n"
    "Options:\n"
    "\t-s\tSplitscreen mode. Plays both recorded screens for a given lecture. "
    "Does not discriminate if these recordings are identical.\n"
    "\t-t\tStart time of recording. [timestamp] format is HH:MM:SS\n"
    "\t-l\tLecture to be played. 1 indexed, given as a number\n";

/*  FUNCTION DEFS  */

int parse(char** argv, Parameters* params);
bool check_time_arg(char* time);

/*  FUNCTIONALITY  */

int main(int argc, char** argv)
{
    int exitCode = 0;
    Options options;
    Parameters params = { .options = options};

    if ((exitCode = parse(argv, &params))) {
        return exitCode;
    }
    if ((exitCode = fetch_lecture(&params))) {
        return exitCode;
    }

    return 0;
}

int parse(char** argv, Parameters* params)
{
    argv++; // Don't care about function name we know what function it is
    while (argv[0]) {
        if (!strcmp(argv[0], "-l") && argv[1]) {
            params->lecture = argv[1];
            argv++;
        }
        } else if (!strcmp(argv[0], "-s")) {
            params->options.splitScreen = true;
        } else if (!strcmp(argv[0], "-t") && argv[1]) {
            if (!check_time_arg(argv[1])) {
                printf(timeUsage);
                return BAD_TIME;
            }
            params->options.startTime = argv[1];
            argv++;
        } else if (!strcmp(argv[0], "-c") && argv[1]) {
            params->course = argv[1];
            argv++;
        }
        } else {
            printf(usage);
            return BAD_USAGE;
        }
        argv++;
    }
    if (!params->lecture) {
        printf(usage);
        return BAD_USAGE;
    }
    return GOOD;
}

bool check_time_arg(char* time)
{
    // Check if of HH:MM:SS format
    // HH:MM:SS is 8 characters long **MAGIC NUMBER ALERT**
    if (strlen(time) != 8) {
        return false;
    }
    // Check for 'HH:'
    for (int i = 0; i < 8; i++) {
        // AHHHHHHH MAGIC NUMBERS LOOK AT HOW BAD THE CODE IS OMG
        if (i == 2 || i == 5) {
            if (time[i] != ':') {
                return false;
            }
        } else {
            if (!isdigit(time[i])) {
                return false;
            }
        }
    }
    return true;
}

int fetch_lecture(Parameters* params)
{
    char* buf;
    findcwd(&buf);
    char* temp = "fetcher.py";
    strcat

    if (!fork()) {
        execlp("python3", 
                "python3", temp, params->course, params->lecture, NULL);
        // If exec fails
}
