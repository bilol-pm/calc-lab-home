// Biloliddin Gulomov 251ADB096
// https://github.com/bilol-pm/calc-lab-home
// Compile with: gcc -O2 -Wall -Wextra -std=c17 -o calc calc.c -lm
// Resources used: AI (Claude, claude.ai) - chat link: KEYIN-QOYAMIZ

#include <stdio.h>

int main(int argc, char **argv) {
    if (argc != 2) {
        fprintf(stderr, "usage: calc input.txt\n");
        return 1;
    }
    FILE *f = fopen(argv[1], "rb");
    if (f == NULL) {
        fprintf(stderr, "calc: cannot open '%s'\n", argv[1]);
        return 1;
    }
    printf("file opened\n");
    fclose(f);
    return 0;
}