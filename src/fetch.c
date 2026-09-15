#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <errno.h>

#include <sys/wait.h>

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
 *      BAD     given any syscalls fail.
 *     [python script error codes]
 *      GOOD    upon success
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

