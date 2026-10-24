// Biloliddin Gulomov 251ADB096
// https://github.com/bilol-pm/calc-lab-home
// Compile with: gcc -O2 -Wall -Wextra -std=c17 -o calc calc.c -lm
// Resources used: AI (Claude, claude.ai) - chat link: KEYIN-QOYAMIZ

#include <stdio.h>
#include <stdlib.h>

typedef enum {
    T_INT,     // a number like 25
    T_PLUS,    // +
    T_MINUS,   // -
    T_EOL,     // end of the line: a '\n' or the end of the file
    T_BAD      // any other character
} TokenType;

typedef struct {
    TokenType type;
    size_t start;   // index of the first character of the token
    size_t end;     // index just after the last character
} Token;

// Looks at the next token, starting from index pos. It does NOT move anything.
static Token peek(const char *text, size_t len, size_t pos) {
    size_t i = pos;
    while (i < len && (text[i] == ' ' || text[i] == '\t')) i++;   // skip blanks

    Token t = { T_BAD, i, i + 1 };
    if (i >= len || text[i] == '\n') {
        t.type = T_EOL;
        t.end = i;
    } else if (text[i] == '+') {
        t.type = T_PLUS;
    } else if (text[i] == '-') {
        t.type = T_MINUS;
    } else if (text[i] >= '0' && text[i] <= '9') {
        size_t j = i;
        while (j < len && text[j] >= '0' && text[j] <= '9') j++;
        t.type = T_INT;
        t.end = j;
    }
    return t;
}

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
        // Temporary: print every token, to see how the tokenizer works.
    const char *names[] = { "INT", "PLUS", "MINUS", "EOL", "BAD" };
    size_t pos = 0;
    for (;;) {
        Token t = peek(text, len, pos);
        printf("%-5s start=%zu end=%zu\n", names[t.type], t.start, t.end);
        if (t.type == T_EOL && t.start >= len) break;   // end of the file
        if (t.type == T_EOL) pos = t.start + 1;         // skip the '\n'
        else pos = t.end;                               // go after the token
    }
    free(text);
    return 0;
}