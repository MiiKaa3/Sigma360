/**
 * @file: error.c
 * @author: sammado103
 * @breif: error handling functions.
 */

#include <stdio.h>
#include <string.h>

#include "const.h"

const char* const errorBad =
    "FATAL: some system call or fundamental operation failled during runtime.\n"
    "Failure point at: %s\n"
    "Cause might range from malloc failure to notcurses display allocation "
    "failure. Verify device integrity and if error persists note as bug.\n";

const char* const errorBadCookies =
    "FATAL: cookies were unable to be fetched. Please verify program "
    "dependencies are up to date and device has stable internet connection. "
    "Please note as bug if above is guarenteed.\n";




int write_fatal_error(int exitCode, char* info)
{
    switch(exitCode) {
        case BAD:
            fprintf(stderr, errorBad, info);
            break;
        case BAD_COOKIES:
            fprintf(stderr, errorBadCookies);
            break;
         default: 
            break;
    }
    return exitCode;
}
