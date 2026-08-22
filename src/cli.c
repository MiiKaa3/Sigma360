#include "cli.h"
#include "utilities.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <time.h>
#include <locale.h>
#include <langinfo.h>

void sigma360_welcome_msg(void);
void sigma360_prompt(void);
void set_terminal_props(void);
void sigma360_watch(void);
const char *C_RESET, *C_RED, *C_GREEN, *C_BLUE, *D_BAR, *D_EMPTY, *U_HIDE, *U_SHOW;

void sigma360_cli(void) {
    char* root;
    if (build_tree(&root)) {
        fprintf(stderr, "Bad temp tree construction");
        exit(-1);
    }
    sigma360_welcome_msg();
    set_terminal_props();

    char input[256];

    while (1) {
        sigma360_prompt();
        if (fgets(input, sizeof(input), stdin) == NULL) {
            printf("\n");
            break;
        }

        // Remove trailing newline character
        input[strcspn(input, "\n")] = 0;

        if (strcmp(input, "exit") == 0) {
            break;
        } else if (strcmp(input, "help") == 0) {
            printf("Available commands:\n");
            printf("%s  help%s  - Show this help message\n", C_BLUE, C_RESET);
            printf("%s  exit%s  - Exit the CLI\n", C_BLUE, C_RESET);
            printf("%s  clear%s - Clear the terminal screen\n", C_BLUE, C_RESET);
            printf("%s  pwd%s   - Print the current working directory\n", C_BLUE, C_RESET);
            printf("%s  ls%s    - List files in the current directory\n", C_BLUE, C_RESET);
            printf("%s  cd%s    - Change the current directory\n", C_BLUE, C_RESET);
        } else if (strcmp(input, "pwd") == 0) {
            printf("/\n");
            printf("%sNOT IMPLEMENTED%s\n", C_RED, C_RESET);
        } else if (strcmp(input, "ls") == 0) {
            printf("%sCourse 1\nCourse 2\nCourse 3\nCourse 4%s\n", C_BLUE, C_RESET);
            printf("%sNOT IMPLEMENTED%s\n", C_RED, C_RESET);
        } else if (strcmp(input, "cd") == 0) {
            printf("%sNOT IMPLEMENTED%s\n", C_RED, C_RESET);
        } else if (strcmp(input, "clear") == 0) {
            printf("\033[H\033[J");
        } else if (strcmp(input, "") == 0) {
            // Do nothing for empty input
        } else if (strcmp(input, "watch") == 0) {
            sigma360_watch();
            printf("%sNOT IMPLEMENTED%s\n", C_RED, C_RESET);
        }
        else {
            printf("Unknown command: %s\n", input);
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

void sigma360_watch(void) {
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
    printf("Watching...\n");
    usleep(2000000);
    printf("%s", U_SHOW);
}
