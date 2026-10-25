// Biloliddin Gulomov 251ADB096
// https://github.com/bilol-pm/calc-lab-home
// Compile with: gcc -O2 -Wall -Wextra -std=c17 -o calc calc.c -lm
// Resources used: AI (Claude, claude.ai) - chat link: https://claude.ai/share/5f3c3b9f-a0b8-4a6a-998f-9a94a94b1513

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <math.h>
#include <dirent.h>
#include <sys/stat.h>

// These words are part of every output file name.
#define NAME "biloliddin"
#define LASTNAME "gulomov"
#define STUDENT_ID "251ADB096"

typedef enum {
    T_INT,     // an integer like 25
    T_FLOAT,   // a float like 12.5
    T_PLUS,    // +
    T_MINUS,   // -
    T_STAR,    // *
    T_SLASH,   // /
    T_POW,     // **
    T_LPAREN,  // (
    T_RPAREN,  // )
    T_EOL,     // end of the line: a '\n' or the end of the file
    T_BAD      // any other character
} TokenType;

typedef struct {
    TokenType type;
    size_t start;   // index of the first character of the token
    size_t end;     // index just after the last character
} Token;

// A number is either an integer or a float. We remember which one, because
// "7 / 2" must give 3 (integer) but "7 / 2.0" must give 3.5 (float).
typedef struct {
    int is_float;   // 0: the number is in i, 1: the number is in d
    long long i;
    double d;
} Value;

typedef struct {
    char *text;         // the whole file (we change it for a moment in number_value)
    size_t len;         // its length
    size_t pos;         // index where we continue reading
    size_t err;         // 0 = no error yet, otherwise the 1-based position of the first error
} Parser;

// Remember the position of the FIRST error. The position is index + 1,
// because the rules count from 1 and the computer counts from 0.
static void fail(Parser *p, size_t index) {
    if (p->err == 0) p->err = index + 1;
}

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
    } else if (s[i] == '*') {
        // Two stars are ONE token. If we read them as two '*' tokens,
        // "2 ** 3" and "2 * * 3" would look the same.
        if (i + 1 < len && s[i + 1] == '*') {
            t.type = T_POW;
            t.end = i + 2;
        } else {
            t.type = T_STAR;
        }
    } else if (s[i] == '/') {
        t.type = T_SLASH;
    } else if (s[i] == '(') {
        t.type = T_LPAREN;
    } else if (s[i] == ')') {
        t.type = T_RPAREN;
    } else if (s[i] >= '0' && s[i] <= '9') {
        size_t j = i;
        while (j < len && s[j] >= '0' && s[j] <= '9') j++;
        t.type = T_INT;
        // A '.' belongs to the number only if a digit follows it. Otherwise the
        // number ends before the '.', and the '.' becomes a T_BAD token of its
        // own. So "5.", ".5" and "1.5e3" give an error exactly at the '.' or
        // at the 'e', as the rules say.
        if (j + 1 < len && s[j] == '.' && s[j + 1] >= '0' && s[j + 1] <= '9') {
            j++;
            while (j < len && s[j] >= '0' && s[j] <= '9') j++;
            t.type = T_FLOAT;
        }
        t.end = j;
    }
    return t;
}

// Output buffer: we keep all results in memory and write them only at the end.
// Why: if any line has an error, the output must contain ONLY "ERROR:<pos>".
typedef struct {
    char *data;     // the text collected so far
    size_t len;     // how many characters are in it
    size_t cap;     // how many characters fit before we must make it bigger
} Out;

// Adds n characters to the buffer. When the buffer is full we make it twice
// bigger, so we copy the data only a few times, even for a huge file.
static void out_add(Out *o, const char *s, size_t n) {
    if (n == 0) return;
    if (o->len + n > o->cap) {
        size_t cap = o->cap ? o->cap : 4096;
        while (cap < o->len + n) cap *= 2;
        char *bigger = realloc(o->data, cap);
        if (bigger == NULL) {
            fprintf(stderr, "calc: out of memory\n");
            exit(1);
        }
        o->data = bigger;
        o->cap = cap;
    }
    memcpy(o->data + o->len, s, n);
    o->len += n;
}

// Reads the whole file into one block of memory.
// AI (Claude): I asked how to read a whole file in C step by step.
// I used the realloc doubling idea: when the buffer is full, make it twice bigger.
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

// Turns a number token into a Value.
static Value number_value(Parser *p, Token t) {
    Value v = { 0, 0, 0.0 };
    if (t.type == T_INT) {
        // Unsigned arithmetic never has undefined behaviour, even for huge input.
        unsigned long long n = 0;
        for (size_t k = t.start; k < t.end; k++)
            n = n * 10 + (unsigned long long)(p->text[k] - '0');
        v.i = (long long)n;
        return v;
    }
    // strtod would also read things after our digits (like "e3" in "1.5e3"),
    // so for a moment we cut the text with a '\0' right after the token. We own
    // this buffer, so changing it and putting the old character back is safe.
    char saved = p->text[t.end];
    p->text[t.end] = '\0';
    v.is_float = 1;
    v.d = strtod(p->text + t.start, NULL);
    p->text[t.end] = saved;
    return v;
}

static double as_double(Value v) {
    return v.is_float ? v.d : (double)v.i;
}

// Integer power by repeated squaring: 3 ** 10 needs only about 4 steps, so
// even a huge exponent is fast. The numbers are unsigned, so a very big result
// wraps around instead of causing undefined behaviour in C.
static long long int_power(long long base, long long exponent) {
    unsigned long long result = 1;
    unsigned long long b = (unsigned long long)base;
    unsigned long long e = (unsigned long long)exponent;
    while (e > 0) {
        if (e & 1) result *= b;
        b *= b;
        e >>= 1;
    }
    return (long long)result;
}

// Computes "a op b". op_pos is the index of the operator and rhs_pos is the
// index where the right operand starts. They are only used for errors:
//   division by zero          -> start of the right operand (the zero is wrong)
//   negative integer exponent -> start of the exponent (the exponent is wrong)
//   float result inf or nan   -> the operator (no single operand is wrong)
static Value apply(Parser *p, TokenType op, Value a, Value b,
                   size_t op_pos, size_t rhs_pos) {
    Value r = { 0, 0, 0.0 };

    // Division by zero, for integers and floats. In C, 0.0 == -0.0 is true.
    if (op == T_SLASH && (b.is_float ? b.d == 0.0 : b.i == 0)) {
        fail(p, rhs_pos);
        return r;
    }

    // Integer or float? If BOTH operands are integers, we stay in integers.
    // If at least one is a float, the other one is converted to double and the
    // whole operation is done in double. This is the rule "7 / 2.0 is 3.5".
    if (!a.is_float && !b.is_float) {
        unsigned long long x = (unsigned long long)a.i;
        unsigned long long y = (unsigned long long)b.i;
        switch (op) {
            case T_PLUS:  r.i = (long long)(x + y); break;
            case T_MINUS: r.i = (long long)(x - y); break;
            case T_STAR:  r.i = (long long)(x * y); break;
            case T_SLASH:
                // C truncates toward zero, as the rules want: 7 / 2 is 3.
                // b == -1 is separate, because the most negative number
                // divided by -1 would overflow.
                r.i = (b.i == -1) ? (long long)(0ULL - x) : a.i / b.i;
                break;
            case T_POW:
                if (b.i < 0) {              // an integer cannot have a negative power
                    fail(p, rhs_pos);
                    return r;
                }
                r.i = int_power(a.i, b.i);  // x ** 0 is 1, also 0 ** 0
                break;
            default: break;
        }
        return r;
    }

    double x = as_double(a), y = as_double(b), z = 0.0;
    switch (op) {
        case T_PLUS:  z = x + y; break;
        case T_MINUS: z = x - y; break;
        case T_STAR:  z = x * y; break;
        case T_SLASH: z = x / y; break;
        case T_POW:   z = pow(x, y); break;
        default: break;
    }
    if (!isfinite(z)) {                     // inf or nan, for example 10.0 ** 400
        fail(p, op_pos);
        return r;
    }
    r.is_float = 1;
    r.d = z;
    return r;
}

// primary and expr call each other (primary -> expr -> term -> primary), so
// the compiler must know that expr exists before primary uses it.
static Value expr(Parser *p);

// primary := NUMBER | '(' expr ')'
// Anything else cannot start a number, so it is an error at the start of that
// token. This one fail() gives all these errors:
//   a bad character ("2 + & 5")  -> at that character
//   two operators ("7 + * 3")    -> at the second operator
//   "-5" (no unary minus)        -> at the '-'
//   missing operand ("3 +")      -> at the '\n' or the end of the file
//   empty parentheses ("2 * ()") -> at the ')'
static Value primary(Parser *p) {
    Value v = { 0, 0, 0.0 };
    Token t = peek(p);

    if (t.type == T_INT || t.type == T_FLOAT) {
        p->pos = t.end;
        return number_value(p, t);
    }

    if (t.type == T_LPAREN) {
        p->pos = t.end;
        // Inside the parentheses there is a whole new expression. Calling expr
        // again (recursion) starts the + - level from the beginning, so
        // "(1 + 2) * 3" is 9: the parentheses are calculated first.
        v = expr(p);
        if (p->err) return v;

        Token close = peek(p);
        if (close.type != T_RPAREN) {
            // A missing ')' at the end of the line: close is the end-of-line
            // token, so the error is at the '\n' (or at the end of the file),
            // as the rules say.
            fail(p, close.start);
            return v;
        }
        p->pos = close.end;
        return v;
    }

    fail(p, t.start);
    return v;
}

// power := primary [ '**' power ]
// power calls ITSELF for the exponent, so 2 ** 3 ** 2 groups as 2 ** (3 ** 2),
// which is 512. That is what "right-associative" means. The loops in term and
// expr go the other way: they use the left result first, so 8 - 3 - 2 is
// (8 - 3) - 2. Power is below term, so ** is stronger than * and /.
static Value power(Parser *p) {
    Value base = primary(p);
    if (p->err) return base;

    Token op = peek(p);
    if (op.type != T_POW) return base;
    p->pos = op.end;

    Token rhs = peek(p);                // the exponent starts here
    Value exponent = power(p);
    if (p->err) return base;
    return apply(p, T_POW, base, exponent, op.start, rhs.start);
}

// term := power { ('*' | '/') power }
// term sits BETWEEN expr and power. This is why * and / are stronger than
// + and -: in "3 + 2 * 5" the expr loop asks term for its right side, and term
// takes "2 * 5" completely before expr can add it to 3.
static Value term(Parser *p) {
    Value v = power(p);
    for (;;) {
        if (p->err) return v;
        Token op = peek(p);
        if (op.type != T_STAR && op.type != T_SLASH) return v;
        p->pos = op.end;

        Token rhs = peek(p);            // the right operand starts here
        Value r = power(p);
        if (p->err) return v;
        v = apply(p, op.type, v, r, op.start, rhs.start);
        if (p->err) return v;
    }
}

// expr := term { ('+' | '-') term }
static Value expr(Parser *p) {
    Value v = term(p);
    for (;;) {
        if (p->err) return v;
        Token op = peek(p);
        if (op.type != T_PLUS && op.type != T_MINUS) return v;
        p->pos = op.end;

        Token rhs = peek(p);
        Value r = term(p);
        if (p->err) return v;
        v = apply(p, op.type, v, r, op.start, rhs.start);
        if (p->err) return v;
    }
}

// Calculates every line of the file and adds each result to out.
// Returns 0 if all is fine, or the 1-based position of the first error.
static size_t evaluate(Parser *p, Out *out) {
    while (p->pos < p->len) {
        Token t = peek(p);

        if (t.type == T_EOL) {              // blank line: nothing to calculate
            p->pos = t.start;
            if (p->pos < p->len) p->pos++;  // skip the '\n'
            continue;
        }

        if (t.type == T_BAD && p->text[t.start] == '#') {
            // '#' is a comment only if it is the first character on the line
            // that is not a blank. In the middle of a line the parser sees it
            // and reports it as an invalid character. We skip the whole
            // comment with memchr, but p->pos still moves over every
            // character, so the positions after the comment stay correct.
            const char *nl = memchr(p->text + t.start, '\n', p->len - t.start);
            p->pos = nl ? (size_t)(nl - p->text) + 1 : p->len;
            continue;
        }

        Value value = expr(p);
        if (!p->err) {
            // The expression is finished, so only the end of the line may follow.
            // Anything else is an error, for example "2 3".
            Token end = peek(p);
            if (end.type != T_EOL) fail(p, end.start);
        }
        if (p->err) return p->err;

        // Integers are printed with %lld, floats with %.15g (so 1.25 * 4 prints
        // 5 and 0.1 + 0.2 prints 0.3). 64 characters are more than enough.
        char line[64];
        int n = value.is_float ? snprintf(line, sizeof line, "%.15g\n", value.d)
                               : snprintf(line, sizeof line, "%lld\n", value.i);
        out_add(out, line, (size_t)n);

        p->pos = peek(p).start;             // now at the '\n' or the end of the file
        if (p->pos < p->len) p->pos++;      // skip the '\n'
    }
    return 0;
}

// File name without the folder and without ".txt": "labs/task1.txt" -> "task1".
// We return a pointer into the same string and its length, so nothing is copied.
static const char *file_base(const char *path, size_t *n) {
    const char *slash = strrchr(path, '/');
    const char *name = slash ? slash + 1 : path;
    *n = strlen(name);
    if (*n > 4 && strcmp(name + *n - 4, ".txt") == 0) *n -= 4;
    return name;
}

// Builds the path  <OUTDIR>/<base>_<name>_<lastname>_<studentid>.txt
// The first snprintf only measures how long the text will be, the second one
// writes it into memory of exactly the right size.
static char *output_path(const char *outdir, const char *base, size_t base_len) {
    int n = snprintf(NULL, 0, "%s/%.*s_%s_%s_%s.txt", outdir, (int)base_len, base,
                     NAME, LASTNAME, STUDENT_ID);
    char *path = malloc((size_t)n + 1);
    if (path == NULL) {
        fprintf(stderr, "calc: out of memory\n");
        exit(1);
    }
    snprintf(path, (size_t)n + 1, "%s/%.*s_%s_%s_%s.txt", outdir, (int)base_len, base,
             NAME, LASTNAME, STUDENT_ID);
    return path;
}

// Creates the output folder. If it already exists we just use it.
static int ensure_dir(const char *path) {
    if (mkdir(path, 0777) == 0) return 0;
    int saved = errno;
    struct stat st;
    if (saved == EEXIST && stat(path, &st) == 0 && S_ISDIR(st.st_mode)) return 0;
    fprintf(stderr, "calc: cannot create output folder '%s': %s\n", path,
            saved == EEXIST ? "exists but is not a folder" : strerror(saved));
    return 1;
}

// Calculates one input file and writes the result file into outdir.
// An ERROR:<pos> result is normal: it returns 0. Only real problems
// (cannot read the input, cannot write the output) return 1.
static int process_file(const char *input_path, const char *outdir) {
    size_t len = 0;
    char *text = read_file(input_path, &len);
    if (text == NULL) {
        fprintf(stderr, "calc: cannot read '%s': %s\n", input_path, strerror(errno));
        return 1;
    }

    Parser p = { text, len, 0, 0 };
    Out out = { NULL, 0, 0 };
    size_t err = evaluate(&p, &out);
    if (err) {
        // "No partial results": throw away everything and keep only the error.
        char line[64];
        int n = snprintf(line, sizeof line, "ERROR:%zu\n", err);
        out.len = 0;
        out_add(&out, line, (size_t)n);
    }
    free(text);

    int rc = 1;
    if (ensure_dir(outdir) == 0) {
        size_t base_len;
        const char *base = file_base(input_path, &base_len);
        char *path = output_path(outdir, base, base_len);
        FILE *f = fopen(path, "wb");
        if (f == NULL) {
            fprintf(stderr, "calc: cannot write '%s': %s\n", path, strerror(errno));
        } else {
            if (out.len > 0) fwrite(out.data, 1, out.len, f);
            if (fclose(f) != 0) fprintf(stderr, "calc: error while writing '%s'\n", path);
            else rc = 0;
        }
        free(path);
    }
    free(out.data);
    return rc;
}

// Last part of a path, ignoring slashes at the end: "labs/week1/" -> "week1".
// We return a pointer into the same string and its length, so nothing is copied.
static const char *last_part(const char *path, size_t *n) {
    size_t end = strlen(path);
    while (end > 0 && path[end - 1] == '/') end--;
    size_t start = end;
    while (start > 0 && path[start - 1] != '/') start--;
    *n = end - start;
    return path + start;
}

// Makes a new copy of a string. The caller must free it.
static char *copy_string(const char *s) {
    size_t n = strlen(s);
    char *c = malloc(n + 1);
    if (c == NULL) {
        fprintf(stderr, "calc: out of memory\n");
        exit(1);
    }
    memcpy(c, s, n + 1);
    return c;
}

// "dir" + "name" -> "dir/name" in new memory. The caller must free it.
static char *join_path(const char *dir, const char *name) {
    int n = snprintf(NULL, 0, "%s/%s", dir, name);
    char *path = malloc((size_t)n + 1);
    if (path == NULL) {
        fprintf(stderr, "calc: out of memory\n");
        exit(1);
    }
    snprintf(path, (size_t)n + 1, "%s/%s", dir, name);
    return path;
}

// Used by qsort to sort file names with strcmp.
static int compare_names(const void *a, const void *b) {
    return strcmp(*(char *const *)a, *(char *const *)b);
}

// Calculates every *.txt file directly inside dir. Other files and
// subfolders are ignored.
static int process_dir(const char *dir, const char *outdir) {
    DIR *d = opendir(dir);
    if (d == NULL) {
        fprintf(stderr, "calc: cannot open folder '%s': %s\n", dir, strerror(errno));
        return 1;
    }

    // First collect the names, then work. Why: if OUTDIR is the same folder as
    // DIR, the files we write must not be read again as inputs.
    char **names = NULL;
    size_t count = 0, cap = 0;
    struct dirent *e;
    while ((e = readdir(d)) != NULL) {
        size_t n = strlen(e->d_name);
        if (n <= 4 || strcmp(e->d_name + n - 4, ".txt") != 0) continue;

        // A folder can also be called "x.txt", so we ask the system what it is.
        char *full = join_path(dir, e->d_name);
        struct stat st;
        int is_file = stat(full, &st) == 0 && S_ISREG(st.st_mode);
        free(full);
        if (!is_file) continue;

        if (count == cap) {
            cap = cap ? cap * 2 : 16;
            char **bigger = realloc(names, cap * sizeof *names);
            if (bigger == NULL) {
                fprintf(stderr, "calc: out of memory\n");
                exit(1);
            }
            names = bigger;
        }
        names[count++] = copy_string(e->d_name);
    }
    closedir(d);

    // readdir gives the names in a random order. We sort them, so the program
    // does the same thing on every run.
    if (count > 1) qsort(names, count, sizeof *names, compare_names);

    // One bad file must not stop the others, but the exit code must show it.
    int dir_ok = ensure_dir(outdir) == 0;
    int rc = dir_ok ? 0 : 1;
    for (size_t k = 0; k < count; k++) {
        if (dir_ok) {
            char *full = join_path(dir, names[k]);
            if (process_file(full, outdir) != 0) rc = 1;
            free(full);
        }
        free(names[k]);
    }
    free(names);
    return rc;
}

// <base>_<name>_<studentid>   (the default output folder when -o is not given)
static char *default_outdir(const char *base, size_t base_len) {
    int n = snprintf(NULL, 0, "%.*s_%s_%s", (int)base_len, base, NAME, STUDENT_ID);
    char *path = malloc((size_t)n + 1);
    if (path == NULL) {
        fprintf(stderr, "calc: out of memory\n");
        exit(1);
    }
    snprintf(path, (size_t)n + 1, "%.*s_%s_%s", (int)base_len, base, NAME, STUDENT_ID);
    return path;
}

// Prints the usage text to stderr (errors go to stderr, not to stdout).
// The return value 2 is the exit code for "wrong command line".
static int usage(const char *message) {
    if (message != NULL) fprintf(stderr, "calc: %s\n", message);
    fprintf(stderr, "usage: calc [-d DIR | --dir DIR] [-o OUTDIR | --output-dir OUTDIR] [input.txt]\n");
    return 2;
}

int main(int argc, char **argv) {
    const char *dir = NULL;
    const char *outdir = NULL;
    const char *input = NULL;

    // Go through the words of the command line one by one. An option that
    // needs a value (like -o) takes the NEXT word, so we move i one more step.
    for (int i = 1; i < argc; i++) {
        const char *a = argv[i];
        if (strcmp(a, "-d") == 0 || strcmp(a, "--dir") == 0) {
            if (i + 1 >= argc) {
                fprintf(stderr, "calc: missing value after '%s'\n", a);
                return usage(NULL);
            }
            i++;
            dir = argv[i];
        } else if (strcmp(a, "-o") == 0 || strcmp(a, "--output-dir") == 0) {
            if (i + 1 >= argc) {
                fprintf(stderr, "calc: missing value after '%s'\n", a);
                return usage(NULL);
            }
            i++;
            outdir = argv[i];
        } else if (a[0] == '-' && a[1] != '\0') {
            fprintf(stderr, "calc: unknown option '%s'\n", a);
            return usage(NULL);
        } else if (input != NULL) {
            fprintf(stderr, "calc: only one input file is allowed, but also got '%s'\n", a);
            return usage(NULL);
        } else {
            input = a;
        }
    }
    if (dir != NULL && input != NULL) return usage("use either -d DIR or an input file, not both");
    if (dir == NULL && input == NULL) return usage("no input file or folder given");

    // Without -o the output folder is <input_base>_<name>_<studentid> in the
    // current folder. With -d, <input_base> is the name of the folder.
    char *auto_dir = NULL;
    if (outdir == NULL) {
        size_t n;
        const char *base = dir ? last_part(dir, &n) : file_base(input, &n);
        auto_dir = default_outdir(base, n);
        outdir = auto_dir;
    }

    int rc = dir ? process_dir(dir, outdir) : process_file(input, outdir);
    free(auto_dir);
    return rc;
}