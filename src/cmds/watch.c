#include <stdio.h>
#include <stdbool.h>
#include <string.h>
#include <ctype.h>

/*  STRUCTS  */

typedef struct {
    bool splitScreen; // -s
    char* startTime; // -t [timestamp as HH:MM:SS]
} Options;

typedef struct {
    char* lecture;
    Options options;
} Parameters;

/*  MACROS  */

#define GOOD      0
#define BAD_USAGE 1
#define BAD_TIME  2

/*  STRING CONSTANTS  */

const char* const usage = 
    "Usage: watch [-s, -t [timestamp]] [lecture]\n"
    "Note that the lecture to watch must be the final argument.\n";

const char* const timeUsage = 
    "Start time argument is to be given as HH:MM:SS\n";

const char* const help =
    "play selected lecture recording. Uses mpv as video player. If this is not "
    "installed, this will command will fail. Final argument to command, if not "
    "an option, will be taken as lecture to watch.\n"
    "Usage: watch [-s, -t [timestamp]] [lecture]\n"
    "Options:\n"
    "\t-s\tSplitscreen mode. Plays both recorded screens for a given lecture. "
    "Does not discriminate if these recordings are identical.\n"
    "\t-t\tStart time of recording. [timestamp] format is HH:MM:SS\n";

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
    printf("%s, %d, %s\n", params.lecture, params.options.splitScreen,
            params.options.startTime);

    return 0;
}

int parse(char** argv, Parameters* params)
{
    argv++; // Don't care about function name we know what function it is
    while (argv[0]) {
        if (!argv[1]) {
            if (!strncmp(argv[0], "-", 1)) {
                printf(usage);
                return BAD_USAGE;
            }
            params->lecture = argv[0];
        } else if (!strcmp(argv[0], "-s")) {
            params->options.splitScreen = true;
        } else if (!strcmp(argv[0], "-t") && argv[1]) {
            if (!check_time_arg(argv[1])) {
                printf(timeUsage);
                return BAD_TIME;
            }
            params->options.startTime = argv[1];
            argv++;
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
                printf("here1\n");
                return false;
            }
        } else {
            if (!isdigit(time[i])) {
                printf("%d\n", i);
                return false;
            }
        }
    }
    return true;
}
