/**
 * @file watch.c
 * @author sammado103
 * @brief Source file for standalone executable called "watch". Executes video
 *      watching via mpv. Bespoke to the Sigma360 program. So only plays
 *      videos of certain names. Expects "v1.mp4", "v2.mp4", and "audio.mp4" in
 *      the given directory. See the help message for options.
 */
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

/**
 * Stores the current options flags set by the user via command line.
 */
typedef struct {
    /// set true is -s is given.
    bool splitScreen;
    /// If -t is given with string to follow, set startTime to the given string.
    char* startTime;
} Options;

/**
 * Stores useful parameters of the program to be passed around.
 */
typedef struct {
    /// The name of the lecture that we will be watching.
    char* lecture;
    /// The Options struct holding user flags.
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

static int parse(char** argv, Parameters* params);
static bool check_time_arg(char* time);
static int play_lecture_mpv(Parameters* params);
static double get_mp4_dur(char* path);
static bool check_dur_v_timestamp(double duration, char* timestamp);

/*  FUNCTIONALITY  */

int main(int argc, char** argv)
{
    int exitCode = GOOD;
    Options options = {.startTime = "00;00;00"};
    Parameters params = {.options = options};

    if ((exitCode = parse(argv, &params))) {
        return exitCode;
    }
    if (!is_lec_downloaded(params.lecture) && 
            (exitCode = get_lecture(params.lecture))) {
        return exitCode;
    }
    if ((exitCode = play_lecture_mpv(&params))) {
        return exitCode;
    }

    return GOOD;
}

/**
 * Parses the command line to verify correct usage. Usage is defined by the 
 * usage message. Only one instance of each flag may be present.
 * @param params The parameters for the program. Contains the program's Options
 *      instance.
 * @param argv   Argv directly from command line.
 * @returns
 *      BAD_WATCH_PARSE     given invalid use of program.
 *      GOOD                upon succes.
 */
static int parse(char** argv, Parameters* params)
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

/**
 * Verify that a valid timestamp was given in command line. Needs to be of the
 * format 'HH;MM;SS', where H, M, and S can be substituted for digits 0-9. 
 * Converts the timestamp into a valid format for mpv.
 * @param time The timestamp given by user at command line.
 * @returns True if valid, False if invalid.
 */
static bool check_time_arg(char* time)
{
    // Check if of HH;MM;SS format
    // HH;MM;SS is 8 characters long 
    if (strlen(time) != 8) {
        return false;
    }
    // Check for 'HH;'
    for (int i = 0; i < 8; i++) {
        // Known indices of ';' in a valid timestamp 
        if (i == 2 || i == 5) {
            if (time[i] != ';') {
                return false;
            }
            // Need to convert HH;MM;SS -> HH:MM:SS for mpv
            time[i] = ':';
        } else {
            if (!isdigit(time[i])) {
                return false;
            }
        }
    }
    return true;
}

/**
 * Plays the desired lecture. This information is provided via command line,
 * parsed, and placed in the program's Parameters struct. Creates a child
 * program to play the lecture. Blocks until lecture window is closed.
 * @param params The parameters for the program.
 * @returns
 *      BAD             given any syscall fails.
 *      BAD_TIMESTAMP   given the timestamp exceesd the duration of lecture.
 *      GOOD            upon success.
 */
static int play_lecture_mpv(Parameters* params)
{
    char* tmpPath = strdup(params->lecture);
    tmpPath = realloc(tmpPath, 
            (strlen(tmpPath)+strlen("/%s")+1) * sizeof(char));
    strcat(tmpPath, "/%s");

    char* v2Path = build_args(tmpPath, "v2.mp4");
    char* aud = build_args(tmpPath, "audio.mp4");

    char* v1 = build_args(tmpPath, "v1.mp4");
    char* time = build_args("--start=%s", params->options.startTime);
    char* audio = build_args("--audio-file=%s", aud);
    char* v2 = build_args("--external-file=%s", v2Path);
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
    return GOOD;
}

/**
 * Creates a pipe and executes an ffprobe command to get the duration of 
 * the .mp4 at the given path. Duration is given in seconds.
 * @param path The absolute path of the .mp4 in question.
 * @returns The duration of the .mp4 at the given path in seconds.
 */
static double get_mp4_dur(char* path) 
{
    char* cmd = "ffprobe -v error -show_entries format=duration "
                "-of default=noprint_wrappers=1:nokey=1 %s";
    cmd = build_args(cmd, path);
    
    // Creates a pipe that pushes the output into stdin
    FILE* fp = popen(cmd, "r");
    double duration;
    fscanf(fp, "%lf", &duration);
    pclose(fp);
    free(cmd);
    return duration;
}

/**
 * Verifies whether the given timestamp exceeds the duration of the lecture.
 * @param duration  Duration of the lecture to be watched.
 * @param timestamp Timestampe provided by user at command line.
 * @returns True if the duration exceeds the timestamp, false otherwise.
 */
static bool check_dur_v_timestamp(double duration, char* timestamp)
{
    double hrs = 10 * (timestamp[0] - '0') + (timestamp[1] - '0');
    double mins = 10 * (timestamp[3] - '0') + (timestamp[4] - '0');
    double secs = 10 * (timestamp[6] - '0') + (timestamp[7] - '0');
    double start = (hrs * 3600) + (mins * 60) + secs;
    if (start >= duration) {
        return false;
    }
    return true;
}
