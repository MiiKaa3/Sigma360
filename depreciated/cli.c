#include "cli.h"
#include "utilities.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <time.h>
#include <locale.h>
#include <langinfo.h>

typedef struct {
    char* dot;
    char* dotdot;
} WD;

void sigma360_welcome_msg(void);
void sigma360_prompt(void);
void set_terminal_props(void);
void sigma360_watch(char* dir);
int read_input(char*** argv, int* argc)
const char *C_RESET, *C_RED, *C_GREEN, *C_BLUE, *D_BAR, *D_EMPTY, *U_HIDE, *U_SHOW;

const char* const help = "Available comands:\n"
        "%s  help%s  - Show this help message\n"
        "%s  exit%s  - Exit the CLI\n"
        "%s  clear%s - Clear the terminal screen\n"
        "%s  pwd%s   - Print the current working directory\n"
        "%s  ls%s    - List files in the current directory\n"
        "%s  cd%s    - Change the current directory\n";
const char* const cdUsage = "Usage: cd [course|lecture]\n";

void sigma360_cli(void) {
    char* root;
    WD wd = {.dot = strdup("/")};
    if (build_tree(&root)) {
        fprintf(stderr, "Bad temp tree construction");
        exit(-1);
    }
    sigma360_welcome_msg();
    set_terminal_props();

    while (1) {
        sigma360_prompt();

        char** argv;
        int argc;
        if (read_input(&argv, &argc) && argc == 0) {
            fprintf(stderr, 
                    "Bad Usage, input \"help\" to see the help message\n");
            continue;
        }

        if (!strcmp(argv[0], "exit")) {
            if (argc > 1) {
                fprintf(stderr, "%s", help);
                continue;
            }
            // Need to implement cleanup routine here.
            break;
        } else if (!strcmp(argv[0], "help")) {
            if (argc > 1) {
                fprintf(stderr, "%s", help);
                continue;
            }
            printf("%s", help);
        } else if (!strcmp(argv[0], "pwd")) {
            if (argc > 1) {
                fprintf(stderr, "%s", help);
                continue;
            }
            printf("%s\n", wd);
        } else if (!strcmp(argv[0], "ls")) {
            printf("%sCourse 1\nCourse 2\nCourse 3\nCourse 4%s\n", C_BLUE, C_RESET);
            printf("%sNOT IMPLEMENTED%s\n", C_RED, C_RESET);
        } else if (!strcmp(argv[0], "cd")) {
            if (argc > 2) {
                fprintf(stderr, "%s", cdUsage);
                continue;
            }
            cd(&wd, argv[1]);
        } else if (!strcmp(argv[0], "clear")) {
            if (argc > 1) {
                fprintf(stderr, "%s", help);
                continue;
            }
            printf("\033[H\033[J");
        } else if (!strcmp(argv[0], "")) {
            if (argc > 1) {
                fprintf(stderr, "%s", help);
                continue;
            }
            // Do nothing for empty input
        } else if (!strcmp(argv[0], "watch")) {
            sigma360_watch(&argv);
            printf("%sNOT IMPLEMENTED%s\n", C_RED, C_RESET);
        }
        else {
            printf("Unknown command: %s\n", argv);
        }
    }
}

void sigma360_welcome_msg(void) {
    printf("   _____ _     Welcome to        ____    __   ___  \n"
           "  / ____(_)                     |___ \\  / /  / _ \\ \n"
           " | (___  _  __ _ _ __ ___   __ _  __) |/ /_ | | | |\n"
           "  \\___ \\| |/ _` | '_ ` _ \\ / _` ||__ <| '_ \\| | | |\n"
           "  ____) | | (_| | | | | | | (_| |___) | (_) | |_| |\n"
           " |_____/|_|\\__, |_| |_| |_|\\__,_|____/ \\___/ \\___/ \n"
           "            __/ |                                  \n"
           "           |___/                                   \n");
    printf("Type 'help' for a list of commands.\n");
}

void sigma360_prompt(void){
    printf("%sSigma360> %s", C_GREEN, C_RESET);
}

void set_terminal_props(void) {
    int color_supported = 1;
    int utf8_supported = 1;

    if (getenv("NO_COLOR") != NULL) {
        color_supported = 0;
    }

    if (color_supported) {
        const char *term = getenv("TERM");
        if (term == NULL || strcmp(term, "dumb") == 0) {
            color_supported = 0;
        }
    }

    if (color_supported && !isatty(STDOUT_FILENO)) {
        color_supported = 0;
        utf8_supported = 0;
    }

    setlocale(LC_CTYPE, "");
    const char *codeset = nl_langinfo(CODESET);
    if (codeset == NULL || strstr(codeset, "UTF-8") == NULL) {
        utf8_supported = 0;
    }


    if (color_supported) {
        C_RESET = "\033[0m";
        C_RED = "\033[1;31m";
        C_GREEN = "\033[1;32m";
        C_BLUE = "\033[1;34m";
    } else {
        C_RESET = "";
        C_RED = "";
        C_GREEN = "";
        C_BLUE = "";
    }

    if (utf8_supported) {
        D_BAR = "█";
        D_EMPTY = "░";
    } else {
        D_BAR = "#";
        D_EMPTY = " ";
    }

    U_HIDE = "\033[?25l";
    U_SHOW = "\033[?25h";
}

void sigma360_watch(char* dir) {
    if (is_dir_empty(dir)) {
        printf("%sDownloading video...\n", U_HIDE);

        srand(time(NULL));
        for (int pct = 0; pct <= 100; pct += 10) {
            int filled = (30 * pct) / 100;

            printf("\r[");
            for (int i = 0; i < 30; i++) {
                fputs(i < filled ? D_BAR : D_EMPTY, stdout);
            }
            printf("] %3d%%", pct);
            fflush(stdout);  // needed since there's no newline yet

            if (pct < 100) {
                // random sleep between 200ms and 800ms
                int ms = 200 + rand() % 600;
                usleep(ms * 1000);
            }
        }
        printf("\n");

        printf("Download complete!\n");
    }
    printf("Watching...\n");
    usleep(2000000);
    printf("%s", U_SHOW);
}

int read_input(char*** argv, int* argc)
{
    char* buf = malloc(sizeof(char));
    int size = 0;
    
    *argv = malloc(sizeof(char*));
    *argc = 0;

    char c;
    while ((c = fgetc(stdin)) != '\n') {
        if (c == ' ') {
            buf = realloc(buf, ++size * sizeof(char));
            buf[size - 1] = '\0';
            *argv = realloc(*argv, ++(*argc) * sizeof(char*));
            (*argv)[*argc - 1] = strdup(buf);
            size = 0;
        } else {
            buf = realloc(buf, ++size * sizeof(char));
            buf[size - 1] = c;
        }
    } 
    *argv = realloc(*argv, ++(*argc) * sizeof(char*));
    (*argv)[*argc - 1] = NULL;
    return 0;
}

int cd(WD* wd, char** argv, char* root)
{
    for (char* nextDir = strtok(argv[0], "/"); nextDir; 
                nextDir = strtok(NULL, "/")) {
        if (!strcmp(nextDir, ".")) {
            continue;
        }
        if (!strcmp(nextDir, "..")) {
            if (!strcmp(wd->dot, "/")) {
                fprintf(stderr,
                        "Currently in root directory, nothing above.\n");
                return -1;
            }
            free(wd->dot);
            wd->dot = wd->dotdot;
            wd->dotdot = NULL;
        } else {
            if (!strncmp(nextDir, "Lecture", strlen("Lecture"))) {
                fprintf(stderr, "%s is not a directory. Used the 'watch' "
                        "command to watch this lecture.\n", nextDir);
                return -1;
            }
            char* temp = strdup(root);
            temp  = realloc(temp, (strlen(temp)+strlen("%s")+1)*sizeof(char));
            strcat(temp, "%s");
            temp = buildArgs(temp, argv[0]);

            DIR* checkDir = opendir(temp);
            if (checkDir) {
                // Directory exists
                wd->dot  = realloc(wd->dot, 
                        (strlen(wd->dot)+strlen("%s")+1)*sizeof(char));
                strcat(wd->dot, "%s");
                temp = buildArgs(wd->dot, argv[0]);
            } else if (ENOENT == errno) {
                // Directory does not exist
                fprintf(stderr, 
                        "Directory '%s' does not exist. Use 'ls' to see list "
                        "of valid directories.\n", argv[0]);
                return -1;
            } else {
                fprintf(stderr, "Failled to verify directory status\n");
                return -1;
            }
        }
    }
}
