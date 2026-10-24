// Biloliddin Gulomov 251ADB096
// https://github.com/bilol-pm/calc-lab-home
// Compile with: gcc -O2 -Wall -Wextra -std=c17 -o calc calc.c -lm
// Resources used: AI (Claude, claude.ai) - chat link: KEYIN-QOYAMIZ

#include <stdio.h>
#include <stdlib.h>

// Reads the whole file into one block of memory.
// AI (Claude): prompt "<I asked how to read a whole file in C step by step, and used the realloc doubling idea.>" - idea: double the buffer with realloc.
static char *read_file(const char *path, size_t *len_out) {
    FILE *f = fopen(path, "rb");
    if (f == NULL) return NULL;

    size_t cap = 4096;                 // how many bytes fit in the buffer now
    size_t len = 0;                    // how many bytes we have read
    char *buf = malloc(cap + 1);       // +1 for the '\0' at the end
    if (buf == NULL) { fclose(f); return NULL; }

    for (;;) {
        len += fread(buf + len, 1, cap - len, f);
        if (len < cap) break;          // fread gave less: the file is finished
        cap *= 2;                      // buffer is full: make it twice bigger
        char *bigger = realloc(buf, cap + 1);
        if (bigger == NULL) { free(buf); fclose(f); return NULL; }
        buf = bigger;
    }
    fclose(f);
    buf[len] = '\0';
    *len_out = len;
    return buf;
}

int main(int argc, char **argv) {
    if (argc != 2) {
        fprintf(stderr, "usage: calc input.txt\n");
        return 1;
    }
    size_t len = 0;
    char *text = read_file(argv[1], &len);
    if (text == NULL) {
        fprintf(stderr, "calc: cannot read '%s'\n", argv[1]);
        return 1;
    }
    printf("read %zu bytes:\n%s", len, text);
    free(text);
    return 0;
}