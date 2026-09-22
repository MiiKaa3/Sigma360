/**
 * @file fetch.c
 * @author sammado103
 * @brief Contains functions that request either girlScout.py or fetcher.py
 * to get cookies, thumbnails, and lecture recordings.
 */
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <errno.h>

#include <sys/wait.h>
#include <cjson/cJSON.h>

#include "utilities.h"
#include "const.h"

/**
 * Fetch cookies - or at least check that cookies exist - before opening
 * program. Calls a cookie fetcher program girlscout.py which does enough
 * to collect Echo360 cookies. Generates 'cookies.json' in program root
 * directory upon success.
 * @param progDir the full path pointing to the Sigma360 program root
 * directory.
 * @returns
 *      BAD             given any syscalls fail.
 *      BAD_COOKIES     given girlScout.py fails to get valid cookies.
 *      GOOD            upon success
 */
int get_cookies()
{
    pid_t pid = fork();
    if (pid < 0) {
        return BAD;
    }
    if (!pid) {
        execlp("python3", "python3", girlScout, NULL);
        _exit(BAD);
    } else {
        int status;
        waitpid(pid, &status, 0);
        if (WIFEXITED(status)) {
            return WEXITSTATUS(status);
        } else {
            return BAD;
        }
    }
}

/**
 * Fetch lectures. Uses fetcher.py with "--watch" argument and the temporary
 * directory of the lecture as another argument. Will download both lecture
 * screens if available, as v1.mp4, v2.mp4, and audio.mp3. If no second screen
 * is available, v1.mp4 = v2.mp4.
 * @param lecture Temp directory of lecture.
 * @returns
 *      BAD             given any syscall fails.
 *      BAD_LEC_GET     given failure to get lecture video/audio.
 *      GOOD            upon success.
 */
int get_lecture(char* lecture)
{
    pid_t pid = fork();
    if (pid < 0) {
        return BAD;
    }
    if (!pid) {
        execlp("python3", "python3", fetcher, "--watch", lecture, NULL);
        // If exec fails
        _exit(BAD);
    }
    int status;
    waitpid(pid, &status, 0);
    if (WIFEXITED(status)) {
        return WEXITSTATUS(status);
    }
    return BAD;
}

/**
 * Fetch thumbnails. Uses fetcher.py with "--thumbnail" and /tmp/sigma_XXXXXX
 * root tmp directory as arguments. Will put Echo360 recording thumbnail in
 * the appropriate lecture directory as t.jpg. Given no thumbnail exists, no
 * thumbnail is retrieved.
 * @param root /tmp/sigma_XXXXXX - the root tmp directory of the program
 * @param pid  The process ID of the thumbnail getter child process. Process
 *          will be left to run while Sigma360 is used, rather than waiting. 
 *          Reaped as the program terminates.
 * @returns
 *      BAD         given any syscall fails.
 *      BAD_THUMB   given failure to fetch any thumbnails.
 *      GOOD        upon success.
 */
int get_thumbnails(char* root, pid_t* pid)
{
    *pid = fork();
    if (*pid < 0) {
        return BAD;
    }
    if (!*pid) {
        execlp("python3", "python3", fetcher, "--thumbnail", root, NULL);
        // If exec fails
        _exit(BAD);
    }
    return GOOD;
}

/**
 * Fetches courses.json. Uses fetcher.py with options "--load" and coursesJSON
 * (see const.c) to generate a courses.json file in the root program directory
 * (.../Sigma360). Assumes valid cookies.json exists in root program directory.
 * @returns
 *      BAD         given any syscall fails.
 *      BAD_FETCH   given fetcher.py fails to fetch courses.json.
 *      GOOD        upon success.
 */
int get_courses_json()
{
    pid_t pid = fork();
    if (pid < 0) {
        return BAD;
    }
    if (!pid) {
        execlp("python3", 
                "python3", fetcher, "--load", coursesJSON, NULL);
        // Upon unsuccessful exec
        _exit(BAD);
    }
    int status;
    waitpid(pid, &status, 0);
    if (WIFEXITED(status)) {
        if (WEXITSTATUS(status)) {
            return BAD_FETCH;
        }
    } else {
        return BAD_FETCH;
    }
    return GOOD;
}
