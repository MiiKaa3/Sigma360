/**
 * @file const.c
 * @author sammado103
 * @brief Stores constants used throughout the program. May also store other
 * constants at some point but currently just strings
 */
#include "const.h"

const char* const version = "Sigma360 version 0.1.0\n";

/*  EXECUTABLES & JSONS     */

const char* const defaultImage = "./previewless.jpg";
char* const coursesJSON = "./courses.json";
const char* const fetcher = "./src/cmds/fetcher.py";
char* const watch = "./src/cmds/watch";
const char* const girlScout = "./src/cmds/girlscout.py";

/*  ERROR MESSAGES          */

const char* const badCWD = 
    "Unable to find program current working directory.";

const char* const watchUsage = 
    "Usage: watch [-s, -t [timestamp]] [-l [lecture]]\n"
    "Note that the lecture to watch must be the final argument.\n";

const char* const watchTimeUsage = 
    "Start time argument is to be given as HH;MM;SS\n";

const char* const badTimestamp =
    "Timestamp given exceeds lecture duration.\n";

const char* const mainUsage =
    "Usage: sigma360 [--version | --help]\n"

/*  HELP MESSAGES           */

const char* const mainHelp =
    "Usage: sigma360 [--version | --help]\n"
    "    --version  Show version information\n"
    "    --help     Show this help message\n\n"
    "Use \"sigma360\" with no arguments to enter program\n";
