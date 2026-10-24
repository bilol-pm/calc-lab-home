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

typedef struct {
    const char *text;   // the whole file
    size_t len;         // its length
    size_t pos;         // index where we continue reading
} Parser;

// Looks at the next token, starting from index pos. It does NOT move anything.
// Looks at the next token. It does NOT move: p->pos stays the same.
static Token peek(const Parser *p) {
    const char *s = p->text;
    size_t len = p->len;
    size_t i = p->pos;
    while (i < len && (s[i] == ' ' || s[i] == '\t')) i++;   // skip blanks

    Token t = { T_BAD, i, i + 1 };
    if (i >= len || s[i] == '\n') {
        t.type = T_EOL;
        t.end = i;
    } else if (s[i] == '+') {
        t.type = T_PLUS;
    } else if (s[i] == '-') {
        t.type = T_MINUS;
    } else if (s[i] >= '0' && s[i] <= '9') {
        size_t j = i;
        while (j < len && s[j] >= '0' && s[j] <= '9') j++;
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

// Turns the digits of an INT token into a number: "25" -> 25.
static long long number_value(const Parser *p, Token t) {
    long long n = 0;
    for (size_t k = t.start; k < t.end; k++)
        n = n * 10 + (p->text[k] - '0');
    return n;
}

// expr := number { ('+' | '-') number }
// For now we trust that the input is correct. Errors come in the next step.
static long long expr(Parser *p) {
    Token t = peek(p);
    long long value = number_value(p, t);
    p->pos = t.end;

    for (;;) {
        Token op = peek(p);
        if (op.type != T_PLUS && op.type != T_MINUS) return value;
        p->pos = op.end;

        Token next = peek(p);
        long long right = number_value(p, next);
        p->pos = next.end;

        if (op.type == T_PLUS) value = value + right;
        else value = value - right;
    }
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

    Parser p = { text, len, 0 };
    printf("%lld\n", expr(&p));

    free(text);
    return 0;
}