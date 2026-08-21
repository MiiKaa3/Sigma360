#include <stdio.h>
#include <string.h>

#include "cli.h"
#include "tui.h"

void version_msg(void);
void usage_msg(void);

int main(int argc, char **argv) {
    if (argc > 1) {
        if (strcmp(argv[1], "--version") == 0) {
            version_msg();
        } else if (strcmp(argv[1], "--help") == 0) {
            usage_msg();
        } else if (strcmp(argv[1], "--cli") == 0) {
            sigma360_cli();
        } else if (strcmp(argv[1], "--tui") == 0) {
            sigma360_tui();
        } else {
            fprintf(stderr, "[ERROR] Unknown argument: %s\n", argv[1]);
            usage_msg();
            return 1;
        }
    } else {
        fprintf(stderr, "[ERROR] No arguments provided.\n");
        usage_msg();
        return 1;
    }

    return 0;
}

void version_msg(void){
    printf("Sigma360 version 0.1.0\n");
}

void usage_msg(void) {
    printf("Usage: sigma360 [arguments]\n");
    printf("Arguments:\n");
    printf("  --version   Show version information\n");
    printf("  --help      Show this help message\n");
    printf("  --cli       Enter CLI mode\n");
    printf("  --tui       Enter TUI mode\n");
}