#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <ctype.h>
#include <unistd.h>

#include <sys/wait.h>
#include <libavformat/avformat.h>

#include "../utilities.h"
#include "../fetch.h"
#include "../const.h"

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

/*  STRING CONSTANTS  */

const char* const help =
    "play selected lecture recording. Uses mpv as video player. If this is not "
    "installed, this will command will fail.\n"
    "Usage: watch [-s, -t [timestamp]] [-l [lecture]]\n"
    "Options:\n"
    "\t-s\tSplitscreen mode. Plays both recorded screens for a given lecture. "
    "Does not discriminate if these recordings are identical.\n"
    "\t-t\tStart time of recording. [timestamp] format is HH;MM;SS\n"
    "\t-l\tLecture to be played. 1 indexed, given as a number\n";

/*  FUNCTION DEFS  */

int parse(char** argv, Parameters* params);
bool check_time_arg(char* time);
int play_lecture_mpv(Parameters* params);
double get_mp4_dur(char* path);
bool check_dur_v_timestamp(double duration, char* timestamp);

/*  FUNCTIONALITY  */

int main(int argc, char** argv)
{
    int exitCode = GOOD;
    Options options = {.startTime = "00;00;00"};
    Parameters params = {.options = options};

    if ((exitCode = parse(argv, &params))) {
        return exitCode;
    }
    if (is_dir_empty(params.lecture) && 
            (exitCode = get_lecture(params.lecture))) {
        return exitCode;
    }
    if ((exitCode = play_lecture_mpv(&params))) {
        return exitCode;
    }

    return GOOD;
}

int parse(char** argv, Parameters* params)
{
    argv++; // Don't care about function name we know what function it is
    while (argv[0]) {
        if (!strcmp(argv[0], "-l") && argv[1]) {
            params->lecture = argv[1];
            argv++;
        } else if (!strcmp(argv[0], "-s")) {
            params->options.splitScreen = true;
        } else if (!strcmp(argv[0], "-t") && argv[1]) {
            if (!check_time_arg(argv[1])) {
                fprintf(stderr, watchTimeUsage);
                return BAD_WATCH_PARSE;
            }
            params->options.startTime = argv[1];
            argv++;
        } else if (!strcmp(argv[0], "-c") && argv[1]) {
            params->course = argv[1];
            argv++;
        } else {
            fprintf(stderr, watchUsage);
            return BAD_WATCH_PARSE;
        }
        argv++;
    }
    if (!params->lecture) {
        fprintf(stderr, watchUsage);
        return BAD_WATCH_PARSE;
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
            if (time[i] != ';') {
                return false;
            }
            time[i] = ':';
        } else {
            if (!isdigit(time[i])) {
                return false;
            }
        }
    }
    return true;
}

int play_lecture_mpv(Parameters* params)
{
    char* tmpPath = strdup(params->lecture);
    tmpPath = realloc(tmpPath, 
            (strlen(tmpPath)+strlen("/%s")+1) * sizeof(char));
    strcat(tmpPath, "/%s");

    char* v2Path = buildArgs(tmpPath, "v2.mp4");
    char* aud = buildArgs(tmpPath, "audio.mp4");

    char* v1 = buildArgs(tmpPath, "v1.mp4");
    char* time = buildArgs("--start=%s", params->options.startTime);
    char* audio = buildArgs("--audio-file=%s", aud);
    char* v2 = buildArgs("--external-file=%s", v2Path);
    free(tmpPath);
    free(v2Path);
    free(aud);

    double dur = get_mp4_dur(v1);
    if (!check_dur_v_timestamp(dur, params->options.startTime)) {
        fprintf(stderr, badTimestamp);
        return BAD_TIMESTAMP;
    }

    pid_t pid = fork();
    if (pid < 0) {
        return BAD;
    }
    if (!pid) {
        if (params->options.splitScreen) {
            execlp("mpv", "mpv", "--really-quiet", v1, audio, time, v2, 
                    "--lavfi-complex=[vid1][vid2]hstack[vo]", NULL);
        } else if (params->options.startTime) {
            execlp("mpv", "mpv", "--really-quiet", v1, audio, time, NULL);
        } else {
            execlp("mpv", "mpv", "--really-quiet", v1, audio, time, NULL);
        }
        _exit(BAD);
    }
    free(time);
    free(audio);
    free(v1);
    free(v2);

    int status;
    waitpid(pid, &status, 0);
    if (WIFEXITED(status)) {
        return WEXITSTATUS(status);
    }
    return BAD;
}

double get_mp4_dur(char* path) 
{
    char* cmd = "ffprobe -v error -show_entries format=duration "
                "-of default=noprint_wrappers=1:nokey=1 %s";
    cmd = buildArgs(cmd, path);
    
    // Creates a pipe that pushes the output into stdin
    FILE* fp = popen(cmd, "r");
    double duration;
    // Dirty read from stdin
    fscanf(fp, "%lf", &duration);
    pclose(fp);
    return duration;
}

/*
 * Converts from timestamp in format HH;MM;SS to seconds
 */
bool check_dur_v_timestamp(double duration, char* timestamp)
{
    double hrs = 10*(timestamp[0]-'0') + (timestamp[1]-'0');
    double mins = 10*(timestamp[3]-'0') + (timestamp[4]-'0');
    double secs = 10*(timestamp[6]-'0') + (timestamp[7]-'0');
    double start = (hrs*3600) + (mins*60) + secs;
    if (start >= duration) {
        return false;
    }
    return true;
}
