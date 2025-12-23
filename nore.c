#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <errno.h>

/* ================================= Tokens ================================= */

typedef enum {
    TOKEN_NUMBER,
    TOKEN_IDENTIFIER,

    TOKEN_FUNC,
    TOKEN_VAL,
    TOKEN_MUT,
    TOKEN_RETURN,
    TOKEN_VOID,
    TOKEN_I32,

    TOKEN_PLUS,
    TOKEN_MINUS,
    TOKEN_STAR,
    TOKEN_SLASH,
    TOKEN_EQUALS,

    TOKEN_LPAREN,
    TOKEN_RPAREN,
    TOKEN_LBRACE,
    TOKEN_RBRACE,
    TOKEN_COLON,
    TOKEN_COMMA,

    TOKEN_EOF,
    TOKEN_ERROR
} TokenKind;

typedef struct {
    TokenKind kind;
    const char *start;
    size_t length;
    size_t line;
    size_t column;
} Token;

/* ================================= Lexer ================================== */

typedef struct {
    const char *source;
    const char *start;
    const char *current;
    size_t line;
    size_t column;
} Lexer;

static void lexer_init(Lexer *lexer, const char *source) {
    lexer->source = source;
    lexer->start = source;
    lexer->current = source;
    lexer->line = 1;
    lexer->column = 1;
}

/* ================================== Chars ================================= */

static int is_digit(char c) {
    return c >= '0' && c <= '9';
}

static int is_alpha(char c) {
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z');
}

static int is_alnum(char c) {
    return is_alpha(c) || is_digit(c);
}

/* ================================= Errors ================================= */

void error(const char *fmt, ...) {
    va_list args;
    va_start(args, fmt);
    fprintf(stderr, "Error: ");
    vfprintf(stderr, fmt, args);
    fprintf(stderr, "\n");
    va_end(args);
    exit(1);
}

/* ================================== Files ================================= */

char *read_file(const char *path, size_t *out_length) {
    FILE *file = fopen(path, "rb");
    if (!file) {
        error("Could not open file '%s': %s", path, strerror(errno));
    }

    fseek(file, 0, SEEK_END);
    size_t length = ftell(file);
    fseek(file, 0, SEEK_SET);

    char *buffer = malloc(length + 1);
    if (!buffer) {
        error("Could not allocate memory for file '%s'", path);
    }

    size_t bytes_read = fread(buffer, 1, length, file);
    if (bytes_read != length) {
        error("Could not read file '%s'", path);
    }

    buffer[length] = '\0';
    fclose(file);

    *out_length = length;
    return buffer;
}

/* ================================ Tokenizing ============================== */

static Token make_token(Lexer *lexer, TokenKind kind) {
    Token token;
    token.kind = kind;
    token.start = lexer->start;
    token.length = lexer->current - lexer->start;
    token.line = lexer->line;
    token.column = lexer->column - token.length;
    return token;
}

static Token error_token(Lexer *lexer, const char *message) {
    Token token;
    token.kind = TOKEN_ERROR;
    token.start = message;
    token.length = strlen(message);
    token.line = lexer->line;
    token.column = lexer->column;
    return token;
}

static char peek(Lexer *lexer) {
    return *lexer->current;
}

static char advance(Lexer *lexer) {
    char c = *lexer->current++;
    if (c == '\n') {
        lexer->line++;
        lexer->column = 1;
    } else {
        lexer->column++;
    }
    return c;
}

static void skip_whitespace(Lexer *lexer) {
    for (;;) {
        char c = peek(lexer);
        switch (c) {
            case ' ':
            case '\t':
            case '\r':
            case '\n':
                advance(lexer);
                break;
            default:
                return;
        }
    }
}

static Token scan_number(Lexer *lexer) {
    while (is_digit(peek(lexer))) {
        advance(lexer);
    }
    return make_token(lexer, TOKEN_NUMBER);
}

static TokenKind identify_keyword(const char *start, size_t length) {
    switch (length) {
        case 3:
            if (memcmp(start, "val", 3) == 0) return TOKEN_VAL;
            if (memcmp(start, "mut", 3) == 0) return TOKEN_MUT;
            if (memcmp(start, "i32", 3) == 0) return TOKEN_I32;
            break;
        case 4:
            if (memcmp(start, "func", 4) == 0) return TOKEN_FUNC;
            if (memcmp(start, "void", 4) == 0) return TOKEN_VOID;
            break;
        case 6:
            if (memcmp(start, "return", 6) == 0) return TOKEN_RETURN;
            break;
    }
    return TOKEN_IDENTIFIER;
}

static Token scan_identifier(Lexer *lexer) {
    while (is_alnum(peek(lexer)) || peek(lexer) == '_') {
        advance(lexer);
    }

    size_t length = lexer->current - lexer->start;
    TokenKind kind = identify_keyword(lexer->start, length);

    return make_token(lexer, kind);
}

Token lexer_next_token(Lexer *lexer) {
    skip_whitespace(lexer);

    lexer->start = lexer->current;

    char c = advance(lexer);

    if (c == '\0') {
        return make_token(lexer, TOKEN_EOF);
    }

    if (is_digit(c)) {
        return scan_number(lexer);
    }

    if (is_alpha(c) || c == '_') {
        return scan_identifier(lexer);
    }

    switch (c) {
        case '(': return make_token(lexer, TOKEN_LPAREN);
        case ')': return make_token(lexer, TOKEN_RPAREN);
        case '{': return make_token(lexer, TOKEN_LBRACE);
        case '}': return make_token(lexer, TOKEN_RBRACE);
        case ':': return make_token(lexer, TOKEN_COLON);
        case ',': return make_token(lexer, TOKEN_COMMA);
        case '+': return make_token(lexer, TOKEN_PLUS);
        case '-': return make_token(lexer, TOKEN_MINUS);
        case '*': return make_token(lexer, TOKEN_STAR);
        case '/': return make_token(lexer, TOKEN_SLASH);
        case '=': return make_token(lexer, TOKEN_EQUALS);
    }

    return error_token(lexer, "Unexpected character");
}

/* =================================== Main ================================= */

int main(int argc, char **argv) {
    if (argc != 2) {
        error("Usage: %s <file.nore>", argv[0]);
    }

    size_t length;
    char *source = read_file(argv[1], &length);

    Lexer lexer;
    lexer_init(&lexer, source);

    for (;;) {
        Token token = lexer_next_token(&lexer);

        printf("Line %zu Col %zu: ", token.line, token.column);
        switch (token.kind) {
            case TOKEN_NUMBER:
                printf("NUMBER '%.*s'\n", (int)token.length, token.start);
                break;
            case TOKEN_IDENTIFIER:
                printf("IDENTIFIER '%.*s'\n", (int)token.length, token.start);
                break;

            case TOKEN_FUNC:
                printf("FUNC '%.*s'\n", (int)token.length, token.start);
                break;
            case TOKEN_VAL:
                printf("VAL '%.*s'\n", (int)token.length, token.start);
                break;
            case TOKEN_MUT:
                printf("MUT '%.*s'\n", (int)token.length, token.start);
                break;
            case TOKEN_RETURN:
                printf("RETURN '%.*s'\n", (int)token.length, token.start);
                break;
            case TOKEN_VOID:
                printf("VOID '%.*s'\n", (int)token.length, token.start);
                break;
            case TOKEN_I32:
                printf("I32 '%.*s'\n", (int)token.length, token.start);
                break;

            case TOKEN_PLUS:
                printf("PLUS '%.*s'\n", (int)token.length, token.start);
                break;
            case TOKEN_MINUS:
                printf("MINUS '%.*s'\n", (int)token.length, token.start);
                break;
            case TOKEN_STAR:
                printf("STAR '%.*s'\n", (int)token.length, token.start);
                break;
            case TOKEN_SLASH:
                printf("SLASH '%.*s'\n", (int)token.length, token.start);
                break;
            case TOKEN_EQUALS:
                printf("EQUALS '%.*s'\n", (int)token.length, token.start);
                break;

            case TOKEN_LPAREN:
                printf("LPAREN '%.*s'\n", (int)token.length, token.start);
                break;
            case TOKEN_RPAREN:
                printf("RPAREN '%.*s'\n", (int)token.length, token.start);
                break;
            case TOKEN_LBRACE:
                printf("LBRACE '%.*s'\n", (int)token.length, token.start);
                break;
            case TOKEN_RBRACE:
                printf("RBRACE '%.*s'\n", (int)token.length, token.start);
                break;
            case TOKEN_COLON:
                printf("COLON '%.*s'\n", (int)token.length, token.start);
                break;
            case TOKEN_COMMA:
                printf("COMMA '%.*s'\n", (int)token.length, token.start);
                break;

            case TOKEN_EOF:
                printf("EOF\n");
                free(source);
                return 0;
            case TOKEN_ERROR:
                printf("ERROR '%.*s'\n", (int)token.length, token.start);
                free(source);
                return 1;
        }
    }
}

