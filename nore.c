#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <stdarg.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <sys/wait.h>

/* ================================= Errors ================================= */

#define EXIT_USER_ERROR 1
#define EXIT_RUNTIME 2
#define EXIT_INTERNAL 101

/* Error code group offsets */
#define ERR_GROUP_INTERNAL 1000
#define ERR_GROUP_DRIVER   2000
#define ERR_GROUP_LEXER    3000
#define ERR_GROUP_PARSER   4000
#define ERR_GROUP_SEMANTIC 5000
#define ERR_GROUP_RUNTIME  6000

typedef struct {
    size_t line;
    size_t column;
} SourceLoc;

typedef enum {
    /* Internal errors: I001-I099 */
    ERR_I001_OUT_OF_MEMORY  = ERR_GROUP_INTERNAL + 1,
    ERR_I002_INTERNAL_ERROR = ERR_GROUP_INTERNAL + 2,

    /* Driver errors: D001-D099 */
    ERR_D001_NO_INPUT_FILE      = ERR_GROUP_DRIVER + 1,
    ERR_D002_UNKNOWN_FLAG       = ERR_GROUP_DRIVER + 2,
    ERR_D003_MULTIPLE_INPUTS    = ERR_GROUP_DRIVER + 3,
    ERR_D004_MISSING_OUTPUT_PATH = ERR_GROUP_DRIVER + 4,
    ERR_D005_FILE_NOT_FOUND     = ERR_GROUP_DRIVER + 5,
    ERR_D006_CLANG_FAILED       = ERR_GROUP_DRIVER + 6,

    /* Lexer errors: L001-L099 */
    ERR_L001_INVALID_CHAR = ERR_GROUP_LEXER + 1,

    /* Parser errors: P001-P099 */
    ERR_P001_EXPECTED_EXPRESSION   = ERR_GROUP_PARSER + 1,
    ERR_P002_EXPECTED_RPAREN       = ERR_GROUP_PARSER + 2,
    ERR_P003_EXPECTED_IDENTIFIER   = ERR_GROUP_PARSER + 3,
    ERR_P004_EXPECTED_EQUALS       = ERR_GROUP_PARSER + 4,
    ERR_P005_EXPECTED_STATEMENT    = ERR_GROUP_PARSER + 5,
    ERR_P006_NUMBER_TOO_LARGE      = ERR_GROUP_PARSER + 6,
    ERR_P007_EXPECTED_RBRACE       = ERR_GROUP_PARSER + 7,
    ERR_P008_EXPECTED_LPAREN_IF    = ERR_GROUP_PARSER + 8,
    ERR_P009_EXPECTED_RPAREN_IF    = ERR_GROUP_PARSER + 9,
    ERR_P010_EXPECTED_LBRACE_IF    = ERR_GROUP_PARSER + 10,
    ERR_P011_EXPECTED_LPAREN_WHILE = ERR_GROUP_PARSER + 11,
    ERR_P012_EXPECTED_RPAREN_WHILE = ERR_GROUP_PARSER + 12,
    ERR_P013_EXPECTED_LBRACE_WHILE = ERR_GROUP_PARSER + 13,
    ERR_P014_EXPECTED_COLON        = ERR_GROUP_PARSER + 14,
    ERR_P015_EXPECTED_TYPE         = ERR_GROUP_PARSER + 15,
    ERR_P016_EXPECTED_FUNC         = ERR_GROUP_PARSER + 16,
    ERR_P017_EXPECTED_FUNC_NAME    = ERR_GROUP_PARSER + 17,
    ERR_P018_EXPECTED_LPAREN_FUNC  = ERR_GROUP_PARSER + 18,
    ERR_P019_EXPECTED_RPAREN_FUNC  = ERR_GROUP_PARSER + 19,
    ERR_P020_EXPECTED_COLON_FUNC   = ERR_GROUP_PARSER + 20,
    ERR_P021_EXPECTED_RETURN_TYPE  = ERR_GROUP_PARSER + 21,
    ERR_P022_EXPECTED_EQUALS_FUNC  = ERR_GROUP_PARSER + 22,
    ERR_P023_EXPECTED_LBRACE_FUNC  = ERR_GROUP_PARSER + 23,

    /* Semantic errors: S001-S099 */
    ERR_S001_DUPLICATE_VARIABLE    = ERR_GROUP_SEMANTIC + 1,
    ERR_S002_UNDECLARED_VARIABLE   = ERR_GROUP_SEMANTIC + 2,
    ERR_S003_IMMUTABLE_ASSIGNMENT  = ERR_GROUP_SEMANTIC + 3,
    ERR_S004_BREAK_OUTSIDE_LOOP    = ERR_GROUP_SEMANTIC + 4,
    ERR_S005_CONTINUE_OUTSIDE_LOOP = ERR_GROUP_SEMANTIC + 5,
    ERR_S006_TYPE_MISMATCH         = ERR_GROUP_SEMANTIC + 6,
    ERR_S007_CONDITION_NOT_BOOL    = ERR_GROUP_SEMANTIC + 7,
    ERR_S008_DUPLICATE_FUNCTION    = ERR_GROUP_SEMANTIC + 8,
    ERR_S009_MISSING_MAIN          = ERR_GROUP_SEMANTIC + 9,
    ERR_S010_INVALID_MAIN_SIG      = ERR_GROUP_SEMANTIC + 10,
    ERR_S011_DUPLICATE_PARAM       = ERR_GROUP_SEMANTIC + 11,
    ERR_S012_VOID_PARAM            = ERR_GROUP_SEMANTIC + 12,
    ERR_S013_RETURN_TYPE_MISMATCH  = ERR_GROUP_SEMANTIC + 13,
    ERR_S014_VOID_RETURN_VALUE     = ERR_GROUP_SEMANTIC + 14,
    ERR_S015_MISSING_RETURN_VALUE  = ERR_GROUP_SEMANTIC + 15,
    ERR_S016_UNDEFINED_FUNCTION    = ERR_GROUP_SEMANTIC + 16,
    ERR_S017_WRONG_ARG_COUNT       = ERR_GROUP_SEMANTIC + 17,
    ERR_S018_ARG_TYPE_MISMATCH     = ERR_GROUP_SEMANTIC + 18,
    ERR_S019_DIVISION_BY_ZERO      = ERR_GROUP_SEMANTIC + 19,
    ERR_S020_TYPE_REQUIRED         = ERR_GROUP_SEMANTIC + 20,

    /* Runtime errors: R001-R099 */
    ERR_R001_ASSERTION_FAILED      = ERR_GROUP_RUNTIME + 1,
} ErrorCode;

static const char *error_code_str(ErrorCode code) {
    static char buffer[8];
    char prefix;
    int num;

    if (code >= ERR_GROUP_RUNTIME) {
        prefix = 'R'; num = code - ERR_GROUP_RUNTIME;
    } else if (code >= ERR_GROUP_SEMANTIC) {
        prefix = 'S'; num = code - ERR_GROUP_SEMANTIC;
    } else if (code >= ERR_GROUP_PARSER) {
        prefix = 'P'; num = code - ERR_GROUP_PARSER;
    } else if (code >= ERR_GROUP_LEXER) {
        prefix = 'L'; num = code - ERR_GROUP_LEXER;
    } else if (code >= ERR_GROUP_DRIVER) {
        prefix = 'D'; num = code - ERR_GROUP_DRIVER;
    } else {
        prefix = 'I'; num = code - ERR_GROUP_INTERNAL;
    }

    snprintf(buffer, sizeof(buffer), "%c%03d", prefix, num);
    return buffer;
}

/* Internal errors (bugs, OOM) - exits with 101 */
static void panic(ErrorCode code, const char *fmt, ...) {
    va_list args;
    va_start(args, fmt);
    fprintf(stderr, "internal error[%s]: ", error_code_str(code));
    vfprintf(stderr, fmt, args);
    fprintf(stderr, "\n");
    va_end(args);
    exit(EXIT_INTERNAL);
}

/* User errors without source location (CLI, file I/O) */
static void error(ErrorCode code, const char *fmt, ...) {
    va_list args;
    va_start(args, fmt);
    fprintf(stderr, "error[%s]: ", error_code_str(code));
    vfprintf(stderr, fmt, args);
    fprintf(stderr, "\n");
    va_end(args);
    exit(EXIT_USER_ERROR);
}

/* Global source file path for diagnostics */
static const char *g_source_file = "<unknown>";

/* Error collection for multi-error reporting */
#define MAX_ERRORS 10

typedef struct {
    ErrorCode code;
    size_t line;
    size_t column;
    char message[256];
} CollectedError;

static CollectedError g_errors[MAX_ERRORS];
static size_t g_error_count = 0;
static bool g_had_error = false;

/* Record error for later reporting (does not exit) */
static void diagnostic(ErrorCode code, size_t line, size_t column,
                       const char *fmt, ...) {
    g_had_error = true;

    if (g_error_count < MAX_ERRORS) {
        CollectedError *err = &g_errors[g_error_count++];
        err->code = code;
        err->line = line;
        err->column = column;

        va_list args;
        va_start(args, fmt);
        vsnprintf(err->message, sizeof(err->message), fmt, args);
        va_end(args);
    }
}

/* Print all collected errors and exit */
static void report_errors_and_exit(void) {
    for (size_t i = 0; i < g_error_count; i++) {
        CollectedError *err = &g_errors[i];
        fprintf(stderr, "%s:%zu:%zu: error[%s]: %s\n",
                g_source_file, err->line, err->column,
                error_code_str(err->code), err->message);
    }

    if (g_error_count == MAX_ERRORS) {
        fprintf(stderr, "Too many errors, stopping.\n");
    }

    exit(EXIT_USER_ERROR);
}

/* Check if we should stop (too many errors) */
static bool too_many_errors(void) {
    return g_error_count >= MAX_ERRORS;
}

/* ================================== Files ================================= */

char *read_file(const char *path, size_t *out_length) {
    FILE *file = fopen(path, "rb");
    if (!file) {
        error(ERR_D005_FILE_NOT_FOUND,
                     "Could not open file '%s': %s", path, strerror(errno));
    }

    fseek(file, 0, SEEK_END);
    size_t length = ftell(file);
    fseek(file, 0, SEEK_SET);

    char *buffer = malloc(length + 1);
    if (!buffer) {
        panic(ERR_I001_OUT_OF_MEMORY, "allocating buffer for file '%s'", path);
    }

    size_t bytes_read = fread(buffer, 1, length, file);
    if (bytes_read != length) {
        error(ERR_D005_FILE_NOT_FOUND,
                     "Could not read file '%s'", path);
    }

    buffer[length] = '\0';
    fclose(file);

    *out_length = length;
    return buffer;
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

/* ================================= Tokens ================================= */

typedef enum {
    TOKEN_NUMBER,
    TOKEN_FLOAT,
    TOKEN_IDENTIFIER,

    TOKEN_FUNC,
    TOKEN_VAL,
    TOKEN_MUT,
    TOKEN_RETURN,
    TOKEN_ASSERT,
    TOKEN_IF,
    TOKEN_ELSE,
    TOKEN_WHILE,
    TOKEN_BREAK,
    TOKEN_CONTINUE,
    TOKEN_VOID,
    TOKEN_I64,
    TOKEN_F64,
    TOKEN_BOOL,
    TOKEN_TRUE,
    TOKEN_FALSE,

    TOKEN_PLUS,
    TOKEN_MINUS,
    TOKEN_STAR,
    TOKEN_SLASH,
    TOKEN_EQUALS,
    TOKEN_EQUAL_EQUAL,
    TOKEN_BANG_EQUAL,
    TOKEN_LESS,
    TOKEN_GREATER,
    TOKEN_LESS_EQUAL,
    TOKEN_GREATER_EQUAL,
    TOKEN_AND_AND,
    TOKEN_OR_OR,
    TOKEN_BANG,

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

static const char *token_kind_name(TokenKind kind) {
    switch (kind) {
        case TOKEN_NUMBER:        return "NUMBER";
        case TOKEN_FLOAT:         return "FLOAT";
        case TOKEN_IDENTIFIER:    return "IDENTIFIER";
        case TOKEN_FUNC:          return "FUNC";
        case TOKEN_VAL:           return "VAL";
        case TOKEN_MUT:           return "MUT";
        case TOKEN_RETURN:        return "RETURN";
        case TOKEN_ASSERT:        return "ASSERT";
        case TOKEN_IF:            return "IF";
        case TOKEN_ELSE:          return "ELSE";
        case TOKEN_WHILE:         return "WHILE";
        case TOKEN_BREAK:         return "BREAK";
        case TOKEN_CONTINUE:      return "CONTINUE";
        case TOKEN_VOID:          return "VOID";
        case TOKEN_I64:           return "I64";
        case TOKEN_F64:           return "F64";
        case TOKEN_BOOL:          return "BOOL";
        case TOKEN_TRUE:          return "TRUE";
        case TOKEN_FALSE:         return "FALSE";
        case TOKEN_PLUS:          return "PLUS";
        case TOKEN_MINUS:         return "MINUS";
        case TOKEN_STAR:          return "STAR";
        case TOKEN_SLASH:         return "SLASH";
        case TOKEN_EQUALS:        return "EQUALS";
        case TOKEN_EQUAL_EQUAL:   return "EQUAL_EQUAL";
        case TOKEN_BANG_EQUAL:    return "BANG_EQUAL";
        case TOKEN_LESS:          return "LESS";
        case TOKEN_GREATER:       return "GREATER";
        case TOKEN_LESS_EQUAL:    return "LESS_EQUAL";
        case TOKEN_GREATER_EQUAL: return "GREATER_EQUAL";
        case TOKEN_LPAREN:        return "LPAREN";
        case TOKEN_RPAREN:        return "RPAREN";
        case TOKEN_LBRACE:        return "LBRACE";
        case TOKEN_RBRACE:        return "RBRACE";
        case TOKEN_COLON:         return "COLON";
        case TOKEN_COMMA:         return "COMMA";
        case TOKEN_EOF:           return "EOF";
        case TOKEN_ERROR:         return "ERROR";
        default:                  return "UNKNOWN";
    }
}

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

static Token lexer_make_token(Lexer *lexer, TokenKind kind) {
    Token token;
    token.kind = kind;
    token.start = lexer->start;
    token.length = lexer->current - lexer->start;
    token.line = lexer->line;
    token.column = lexer->column - token.length;
    return token;
}

static Token lexer_error_token(Lexer *lexer, const char *message) {
    Token token;
    token.kind = TOKEN_ERROR;
    token.start = message;
    token.length = strlen(message);
    token.line = lexer->line;
    token.column = lexer->column;
    return token;
}

static char lexer_peek(Lexer *lexer) {
    return *lexer->current;
}

static char lexer_advance(Lexer *lexer) {
    char c = *lexer->current++;
    if (c == '\n') {
        lexer->line++;
        lexer->column = 1;
    } else {
        lexer->column++;
    }
    return c;
}

static void lexer_skip_whitespace(Lexer *lexer) {
    for (;;) {
        char c = lexer_peek(lexer);
        switch (c) {
            case ' ':
            case '\t':
            case '\r':
            case '\n':
                lexer_advance(lexer);
                break;
            default:
                return;
        }
    }
}

static Token lexer_scan_number(Lexer *lexer) {
    while (is_digit(lexer_peek(lexer))) {
        lexer_advance(lexer);
    }
    /* Check for decimal point followed by digit */
    if (lexer_peek(lexer) == '.' && is_digit(lexer->current[1])) {
        lexer_advance(lexer);  /* consume '.' */
        while (is_digit(lexer_peek(lexer))) {
            lexer_advance(lexer);
        }
        return lexer_make_token(lexer, TOKEN_FLOAT);
    }
    return lexer_make_token(lexer, TOKEN_NUMBER);
}

static TokenKind lexer_identify_keyword(const char *start, size_t length) {
    switch (length) {
        case 2:
            if (memcmp(start, "if", 2) == 0) return TOKEN_IF;
            break;
        case 3:
            if (memcmp(start, "val", 3) == 0) return TOKEN_VAL;
            if (memcmp(start, "mut", 3) == 0) return TOKEN_MUT;
            if (memcmp(start, "i64", 3) == 0) return TOKEN_I64;
            if (memcmp(start, "f64", 3) == 0) return TOKEN_F64;
            break;
        case 4:
            if (memcmp(start, "func", 4) == 0) return TOKEN_FUNC;
            if (memcmp(start, "void", 4) == 0) return TOKEN_VOID;
            if (memcmp(start, "else", 4) == 0) return TOKEN_ELSE;
            if (memcmp(start, "bool", 4) == 0) return TOKEN_BOOL;
            if (memcmp(start, "true", 4) == 0) return TOKEN_TRUE;
            break;
        case 5:
            if (memcmp(start, "while", 5) == 0) return TOKEN_WHILE;
            if (memcmp(start, "break", 5) == 0) return TOKEN_BREAK;
            if (memcmp(start, "false", 5) == 0) return TOKEN_FALSE;
            break;
        case 6:
            if (memcmp(start, "return", 6) == 0) return TOKEN_RETURN;
            if (memcmp(start, "assert", 6) == 0) return TOKEN_ASSERT;
            break;
        case 8:
            if (memcmp(start, "continue", 8) == 0) return TOKEN_CONTINUE;
            break;
    }
    return TOKEN_IDENTIFIER;
}

static Token lexer_scan_identifier(Lexer *lexer) {
    while (is_alnum(lexer_peek(lexer)) || lexer_peek(lexer) == '_') {
        lexer_advance(lexer);
    }

    size_t length = lexer->current - lexer->start;
    TokenKind kind = lexer_identify_keyword(lexer->start, length);

    return lexer_make_token(lexer, kind);
}

Token lexer_next_token(Lexer *lexer) {
    lexer_skip_whitespace(lexer);

    lexer->start = lexer->current;

    char c = lexer_advance(lexer);

    if (c == '\0') {
        return lexer_make_token(lexer, TOKEN_EOF);
    }

    if (is_digit(c)) {
        return lexer_scan_number(lexer);
    }

    if (is_alpha(c) || c == '_') {
        return lexer_scan_identifier(lexer);
    }

    switch (c) {
        case '(': return lexer_make_token(lexer, TOKEN_LPAREN);
        case ')': return lexer_make_token(lexer, TOKEN_RPAREN);
        case '{': return lexer_make_token(lexer, TOKEN_LBRACE);
        case '}': return lexer_make_token(lexer, TOKEN_RBRACE);
        case ':': return lexer_make_token(lexer, TOKEN_COLON);
        case ',': return lexer_make_token(lexer, TOKEN_COMMA);
        case '+': return lexer_make_token(lexer, TOKEN_PLUS);
        case '-': return lexer_make_token(lexer, TOKEN_MINUS);
        case '*': return lexer_make_token(lexer, TOKEN_STAR);
        case '/': return lexer_make_token(lexer, TOKEN_SLASH);
        case '=':
            if (lexer_peek(lexer) == '=') {
                lexer_advance(lexer);
                return lexer_make_token(lexer, TOKEN_EQUAL_EQUAL);
            }
            return lexer_make_token(lexer, TOKEN_EQUALS);
        case '!':
            if (lexer_peek(lexer) == '=') {
                lexer_advance(lexer);
                return lexer_make_token(lexer, TOKEN_BANG_EQUAL);
            }
            return lexer_make_token(lexer, TOKEN_BANG);
        case '&':
            if (lexer_peek(lexer) == '&') {
                lexer_advance(lexer);
                return lexer_make_token(lexer, TOKEN_AND_AND);
            }
            return lexer_error_token(lexer, "Expected '&&'");
        case '|':
            if (lexer_peek(lexer) == '|') {
                lexer_advance(lexer);
                return lexer_make_token(lexer, TOKEN_OR_OR);
            }
            return lexer_error_token(lexer, "Expected '||'");
        case '<':
            if (lexer_peek(lexer) == '=') {
                lexer_advance(lexer);
                return lexer_make_token(lexer, TOKEN_LESS_EQUAL);
            }
            return lexer_make_token(lexer, TOKEN_LESS);
        case '>':
            if (lexer_peek(lexer) == '=') {
                lexer_advance(lexer);
                return lexer_make_token(lexer, TOKEN_GREATER_EQUAL);
            }
            return lexer_make_token(lexer, TOKEN_GREATER);
    }

    return lexer_error_token(lexer, "Unexpected character");
}

static void lexer_print_tokens(Lexer *lexer, const char *source) {
    printf("=== TOKENS ===\n");
    Token token;
    do {
        token = lexer_next_token(lexer);
        printf("%-20s", token_kind_name(token.kind));
        if (token.kind == TOKEN_NUMBER || token.kind == TOKEN_FLOAT ||
            token.kind == TOKEN_IDENTIFIER) {
            printf(" '%.*s'", (int)token.length, token.start);
        }
        printf("\n");
    } while (token.kind != TOKEN_EOF);
    printf("\n");

    /* Reinitialize lexer for potential next phase */
    lexer_init(lexer, source);
}

/* ============================== Token Location ============================ */

static SourceLoc token_loc(Token *token) {
    return (SourceLoc){ .line = token->line, .column = token->column };
}

/* ================================= Types ================================== */

typedef enum {
    /* Sentinel for unspecified type */
    TYPE_UNKNOWN = -1,
    /* Concrete types */
    TYPE_I64,
    TYPE_F64,
    TYPE_BOOL,
    TYPE_VOID,
    /* Comptime types (literals only) */
    TYPE_COMPTIME_INT,
    TYPE_COMPTIME_FLOAT,
} Type;

static const char *type_name(Type type) {
    switch (type) {
        case TYPE_UNKNOWN:       return "unknown";
        case TYPE_I64:           return "i64";
        case TYPE_F64:           return "f64";
        case TYPE_BOOL:          return "bool";
        case TYPE_VOID:          return "void";
        case TYPE_COMPTIME_INT:  return "comptime_int";
        case TYPE_COMPTIME_FLOAT: return "comptime_float";
    }
    return "unknown";
}

static bool type_is_comptime(Type type) {
    return type == TYPE_COMPTIME_INT || type == TYPE_COMPTIME_FLOAT;
}

static bool type_is_numeric(Type type) {
    return type == TYPE_I64 || type == TYPE_F64 ||
           type == TYPE_COMPTIME_INT || type == TYPE_COMPTIME_FLOAT;
}

/* Check if 'from' can coerce to 'to' */
static bool type_can_coerce(Type from, Type to) {
    if (from == to) return true;
    if (from == TYPE_COMPTIME_INT && (to == TYPE_I64 || to == TYPE_F64)) return true;
    if (from == TYPE_COMPTIME_FLOAT && to == TYPE_F64) return true;
    return false;
}

/* Resolve result type for numeric binary operation.
 * Returns TYPE_VOID on error (incompatible types). */
static Type resolve_numeric_binary_type(Type left, Type right) {
    /* Both comptime_int */
    if (left == TYPE_COMPTIME_INT && right == TYPE_COMPTIME_INT) {
        return TYPE_COMPTIME_INT;
    }
    /* Both comptime, at least one float */
    if (type_is_comptime(left) && type_is_comptime(right)) {
        return TYPE_COMPTIME_FLOAT;  /* int promotes to float */
    }
    /* comptime_int + concrete → concrete */
    if (left == TYPE_COMPTIME_INT && (right == TYPE_I64 || right == TYPE_F64)) {
        return right;
    }
    if (right == TYPE_COMPTIME_INT && (left == TYPE_I64 || left == TYPE_F64)) {
        return left;
    }
    /* comptime_float + f64 → f64 */
    if (left == TYPE_COMPTIME_FLOAT && right == TYPE_F64) {
        return TYPE_F64;
    }
    if (right == TYPE_COMPTIME_FLOAT && left == TYPE_F64) {
        return TYPE_F64;
    }
    /* comptime_float + i64 → error */
    if ((left == TYPE_COMPTIME_FLOAT && right == TYPE_I64) ||
        (right == TYPE_COMPTIME_FLOAT && left == TYPE_I64)) {
        return TYPE_VOID;  /* error */
    }
    /* Both same concrete type */
    if (left == right && (left == TYPE_I64 || left == TYPE_F64)) {
        return left;
    }
    /* Mixed concrete (i64 + f64) → error */
    return TYPE_VOID;
}

/* ================================== Params ================================ */

typedef struct {
    const char *name_start;
    size_t name_length;
    Type type;
} Parameter;

/* ================================== AST =================================== */

typedef enum {
    AST_NUMBER,
    AST_FLOAT,
    AST_BOOLEAN,
    AST_IDENTIFIER,
    AST_BINARY,
    AST_UNARY,
    AST_FUNC_CALL,
    AST_VAL_DECL,
    AST_MUT_DECL,
    AST_RETURN,
    AST_ASSIGNMENT,
    AST_ASSERT,
    AST_IF,
    AST_WHILE,
    AST_BREAK,
    AST_CONTINUE,
    AST_BLOCK,
    AST_FUNC_DECL,
    AST_PROGRAM
} AstKind;

typedef enum {
    OP_ADD,
    OP_SUB,
    OP_MUL,
    OP_DIV,
    OP_EQ,
    OP_NEQ,
    OP_LT,
    OP_GT,
    OP_LE,
    OP_GE,
    OP_AND,
    OP_OR
} BinaryOp;

typedef enum {
    OP_NEG,
    OP_NOT
} UnaryOp;

typedef struct Ast {
    AstKind kind;
    SourceLoc loc;
    Type expr_type;  /* Filled during type checking */
    union {
        struct {
            long value;
        } number;

        struct {
            double value;
        } float_lit;

        struct {
            bool value;
        } boolean;

        struct {
            const char *start;
            size_t length;
        } identifier;

        struct {
            BinaryOp op;
            struct Ast *left;
            struct Ast *right;
        } binary;

        struct {
            UnaryOp op;
            struct Ast *operand;
        } unary;

        struct {
            const char *name_start;
            size_t name_length;
            struct Ast **arguments;
            size_t arg_count;
        } func_call;

        struct {
            const char *name_start;
            size_t name_length;
            Type type;
            struct Ast *initializer;
        } val_decl;

        struct {
            const char *name_start;
            size_t name_length;
            Type type;
            struct Ast *initializer;
        } mut_decl;

        struct {
            struct Ast *value;
        } return_stmt;

        struct {
            const char *name_start;
            size_t name_length;
            struct Ast *value;
        } assignment;

        struct {
            struct Ast *condition;
        } assert_stmt;

        struct {
            struct Ast *condition;
            struct Ast *then_block;
            struct Ast *else_block;  /* NULL if no else */
        } if_stmt;

        struct {
            struct Ast *condition;
            struct Ast *body;
        } while_stmt;

        struct {
            struct Ast **statements;
            size_t count;
            size_t capacity;
        } block;

        struct {
            const char *name_start;
            size_t name_length;
            Parameter *params;
            size_t param_count;
            Type return_type;
            struct Ast *body;
        } func_decl;

        struct {
            struct Ast **statements;
            size_t count;
            size_t capacity;
        } program;
    } as;
} Ast;

/* ============================== AST Allocators ============================ */

static Ast *ast_make_number(long value, SourceLoc loc) {
    Ast *node = malloc(sizeof(Ast));
    if (!node) {
        panic(ERR_I001_OUT_OF_MEMORY, "allocating AST node");
    }
    node->kind = AST_NUMBER;
    node->loc = loc;
    node->as.number.value = value;
    return node;
}

static Ast *ast_make_float(double value, SourceLoc loc) {
    Ast *node = malloc(sizeof(Ast));
    if (!node) {
        panic(ERR_I001_OUT_OF_MEMORY, "allocating AST node");
    }
    node->kind = AST_FLOAT;
    node->loc = loc;
    node->as.float_lit.value = value;
    return node;
}

static Ast *ast_make_boolean(bool value, SourceLoc loc) {
    Ast *node = malloc(sizeof(Ast));
    if (!node) {
        panic(ERR_I001_OUT_OF_MEMORY, "allocating AST node");
    }
    node->kind = AST_BOOLEAN;
    node->loc = loc;
    node->as.boolean.value = value;
    return node;
}

static Ast *ast_make_identifier(const char *start, size_t length, SourceLoc loc) {
    Ast *node = malloc(sizeof(Ast));
    if (!node) {
        panic(ERR_I001_OUT_OF_MEMORY, "allocating AST node");
    }
    node->kind = AST_IDENTIFIER;
    node->loc = loc;
    node->as.identifier.start = start;
    node->as.identifier.length = length;
    return node;
}

static Ast *ast_make_binary(BinaryOp op, Ast *left, Ast *right, SourceLoc loc) {
    Ast *node = malloc(sizeof(Ast));
    if (!node) {
        panic(ERR_I001_OUT_OF_MEMORY, "allocating AST node");
    }
    node->kind = AST_BINARY;
    node->loc = loc;
    node->as.binary.op = op;
    node->as.binary.left = left;
    node->as.binary.right = right;
    return node;
}

static Ast *ast_make_unary(UnaryOp op, Ast *operand, SourceLoc loc) {
    Ast *node = malloc(sizeof(Ast));
    if (!node) {
        panic(ERR_I001_OUT_OF_MEMORY, "allocating AST node");
    }
    node->kind = AST_UNARY;
    node->loc = loc;
    node->as.unary.op = op;
    node->as.unary.operand = operand;
    return node;
}

static Ast *ast_make_func_call(const char *name_start, size_t name_length,
                               Ast **arguments, size_t arg_count, SourceLoc loc) {
    Ast *node = malloc(sizeof(Ast));
    if (!node) {
        panic(ERR_I001_OUT_OF_MEMORY, "allocating AST node");
    }
    node->kind = AST_FUNC_CALL;
    node->loc = loc;
    node->as.func_call.name_start = name_start;
    node->as.func_call.name_length = name_length;
    node->as.func_call.arguments = arguments;
    node->as.func_call.arg_count = arg_count;
    return node;
}

static Ast *ast_make_val_decl(const char *name_start, size_t name_length,
                              Type type, Ast *initializer, SourceLoc loc) {
    Ast *node = malloc(sizeof(Ast));
    if (!node) {
        panic(ERR_I001_OUT_OF_MEMORY, "allocating AST node");
    }
    node->kind = AST_VAL_DECL;
    node->loc = loc;
    node->as.val_decl.name_start = name_start;
    node->as.val_decl.name_length = name_length;
    node->as.val_decl.type = type;
    node->as.val_decl.initializer = initializer;
    return node;
}

static Ast *ast_make_mut_decl(const char *name_start, size_t name_length,
                              Type type, Ast *initializer, SourceLoc loc) {
    Ast *node = malloc(sizeof(Ast));
    if (!node) {
        panic(ERR_I001_OUT_OF_MEMORY, "allocating AST node");
    }
    node->kind = AST_MUT_DECL;
    node->loc = loc;
    node->as.mut_decl.name_start = name_start;
    node->as.mut_decl.name_length = name_length;
    node->as.mut_decl.type = type;
    node->as.mut_decl.initializer = initializer;
    return node;
}

static Ast *ast_make_return(Ast *value, SourceLoc loc) {
    Ast *node = malloc(sizeof(Ast));
    if (!node) {
        panic(ERR_I001_OUT_OF_MEMORY, "allocating AST node");
    }
    node->kind = AST_RETURN;
    node->loc = loc;
    node->as.return_stmt.value = value;
    return node;
}

static Ast *ast_make_assignment(const char *name_start, size_t name_length,
                                Ast *value, SourceLoc loc) {
    Ast *node = malloc(sizeof(Ast));
    if (!node) {
        panic(ERR_I001_OUT_OF_MEMORY, "allocating AST node");
    }
    node->kind = AST_ASSIGNMENT;
    node->loc = loc;
    node->as.assignment.name_start = name_start;
    node->as.assignment.name_length = name_length;
    node->as.assignment.value = value;
    return node;
}

static Ast *ast_make_assert(Ast *condition, SourceLoc loc) {
    Ast *node = malloc(sizeof(Ast));
    if (!node) {
        panic(ERR_I001_OUT_OF_MEMORY, "allocating AST node");
    }
    node->kind = AST_ASSERT;
    node->loc = loc;
    node->as.assert_stmt.condition = condition;
    return node;
}

static Ast *ast_make_if(Ast *condition, Ast *then_block, Ast *else_block,
                        SourceLoc loc) {
    Ast *node = malloc(sizeof(Ast));
    if (!node) {
        panic(ERR_I001_OUT_OF_MEMORY, "allocating AST node");
    }
    node->kind = AST_IF;
    node->loc = loc;
    node->as.if_stmt.condition = condition;
    node->as.if_stmt.then_block = then_block;
    node->as.if_stmt.else_block = else_block;
    return node;
}

static Ast *ast_make_while(Ast *condition, Ast *body, SourceLoc loc) {
    Ast *node = malloc(sizeof(Ast));
    if (!node) {
        panic(ERR_I001_OUT_OF_MEMORY, "allocating AST node");
    }
    node->kind = AST_WHILE;
    node->loc = loc;
    node->as.while_stmt.condition = condition;
    node->as.while_stmt.body = body;
    return node;
}

static Ast *ast_make_break(SourceLoc loc) {
    Ast *node = malloc(sizeof(Ast));
    if (!node) {
        panic(ERR_I001_OUT_OF_MEMORY, "allocating AST node");
    }
    node->kind = AST_BREAK;
    node->loc = loc;
    return node;
}

static Ast *ast_make_continue(SourceLoc loc) {
    Ast *node = malloc(sizeof(Ast));
    if (!node) {
        panic(ERR_I001_OUT_OF_MEMORY, "allocating AST node");
    }
    node->kind = AST_CONTINUE;
    node->loc = loc;
    return node;
}

static Ast *ast_make_block(SourceLoc loc) {
    Ast *node = malloc(sizeof(Ast));
    if (!node) {
        panic(ERR_I001_OUT_OF_MEMORY, "allocating AST node");
    }
    node->kind = AST_BLOCK;
    node->loc = loc;
    node->as.block.statements = malloc(8 * sizeof(Ast*));
    if (!node->as.block.statements) {
        free(node);
        panic(ERR_I001_OUT_OF_MEMORY, "allocating block statements");
    }
    node->as.block.count = 0;
    node->as.block.capacity = 8;
    return node;
}

static void ast_block_add_statement(Ast *block, Ast *statement) {
    if (block->kind != AST_BLOCK) {
        panic(ERR_I002_INTERNAL_ERROR,
              "ast_block_add_statement called on non-block node");
    }

    if (block->as.block.count >= block->as.block.capacity) {
        size_t new_capacity = block->as.block.capacity * 2;
        Ast **new_statements = realloc(block->as.block.statements,
                                       new_capacity * sizeof(Ast*));
        if (!new_statements) {
            panic(ERR_I001_OUT_OF_MEMORY, "growing block statements");
        }
        block->as.block.statements = new_statements;
        block->as.block.capacity = new_capacity;
    }

    block->as.block.statements[block->as.block.count++] = statement;
}

static Ast *ast_make_program(void) {
    Ast *node = malloc(sizeof(Ast));
    if (!node) {
        panic(ERR_I001_OUT_OF_MEMORY, "allocating AST node");
    }
    node->kind = AST_PROGRAM;
    node->loc = (SourceLoc){ .line = 0, .column = 0 };
    node->as.program.statements = malloc(8 * sizeof(Ast*));
    if (!node->as.program.statements) {
        free(node);
        panic(ERR_I001_OUT_OF_MEMORY, "allocating program statements");
    }
    node->as.program.count = 0;
    node->as.program.capacity = 8;
    return node;
}

static void ast_program_add_statement(Ast *program, Ast *statement) {
    if (program->kind != AST_PROGRAM) {
        panic(ERR_I002_INTERNAL_ERROR,
              "ast_program_add_statement called on non-program node");
    }

    if (program->as.program.count >= program->as.program.capacity) {
        size_t new_capacity = program->as.program.capacity * 2;
        Ast **new_statements = realloc(program->as.program.statements,
                                       new_capacity * sizeof(Ast*));
        if (!new_statements) {
            panic(ERR_I001_OUT_OF_MEMORY, "growing program statements");
        }
        program->as.program.statements = new_statements;
        program->as.program.capacity = new_capacity;
    }

    program->as.program.statements[program->as.program.count++] = statement;
}

static Ast *ast_make_func_decl(const char *name_start, size_t name_length,
                               Parameter *params, size_t param_count,
                               Type return_type, Ast *body, SourceLoc loc) {
    Ast *node = malloc(sizeof(Ast));
    if (!node) {
        panic(ERR_I001_OUT_OF_MEMORY, "allocating AST node");
    }
    node->kind = AST_FUNC_DECL;
    node->loc = loc;
    node->as.func_decl.name_start = name_start;
    node->as.func_decl.name_length = name_length;
    node->as.func_decl.params = params;
    node->as.func_decl.param_count = param_count;
    node->as.func_decl.return_type = return_type;
    node->as.func_decl.body = body;
    return node;
}

static void ast_free(Ast *node) {
    if (!node) return;
    if (node->kind == AST_BINARY) {
        ast_free(node->as.binary.left);
        ast_free(node->as.binary.right);
    }
    if (node->kind == AST_UNARY) {
        ast_free(node->as.unary.operand);
    }
    if (node->kind == AST_FUNC_CALL) {
        for (size_t i = 0; i < node->as.func_call.arg_count; i++) {
            ast_free(node->as.func_call.arguments[i]);
        }
        free(node->as.func_call.arguments);
    }
    if (node->kind == AST_VAL_DECL) {
        ast_free(node->as.val_decl.initializer);
    }
    if (node->kind == AST_MUT_DECL) {
        ast_free(node->as.mut_decl.initializer);
    }
    if (node->kind == AST_RETURN) {
        ast_free(node->as.return_stmt.value);
    }
    if (node->kind == AST_ASSIGNMENT) {
        ast_free(node->as.assignment.value);
    }
    if (node->kind == AST_ASSERT) {
        ast_free(node->as.assert_stmt.condition);
    }
    if (node->kind == AST_IF) {
        ast_free(node->as.if_stmt.condition);
        ast_free(node->as.if_stmt.then_block);
        ast_free(node->as.if_stmt.else_block);
    }
    if (node->kind == AST_BLOCK) {
        for (size_t i = 0; i < node->as.block.count; i++) {
            ast_free(node->as.block.statements[i]);
        }
        free(node->as.block.statements);
    }
    if (node->kind == AST_FUNC_DECL) {
        if (node->as.func_decl.params) {
            free(node->as.func_decl.params);
        }
        ast_free(node->as.func_decl.body);
    }
    if (node->kind == AST_PROGRAM) {
        for (size_t i = 0; i < node->as.program.count; i++) {
            ast_free(node->as.program.statements[i]);
        }
        free(node->as.program.statements);
    }
    free(node);
}

/* ================================= Parser ================================= */

typedef struct {
    Lexer *lexer;
    Token current;
    Token previous;
} Parser;

static void parser_advance(Parser *parser) {
    parser->previous = parser->current;
    parser->current = lexer_next_token(parser->lexer);

    /* Handle lexer errors immediately */
    if (parser->current.kind == TOKEN_ERROR) {
        diagnostic(ERR_L001_INVALID_CHAR, parser->current.line,
                   parser->current.column, "%.*s",
                   (int)parser->current.length, parser->current.start);
    }
}

static void parser_init(Parser *parser, Lexer *lexer) {
    parser->lexer = lexer;
    parser->previous.kind = TOKEN_EOF;
    parser_advance(parser);
}

static int parser_check(Parser *parser, TokenKind kind) {
    return parser->current.kind == kind;
}

static int parser_match(Parser *parser, TokenKind kind) {
    if (!parser_check(parser, kind)) {
        return 0;
    }
    parser_advance(parser);
    return 1;
}

static TokenKind parser_peek_next(Parser *parser) {
    /* Save lexer state */
    Lexer saved = *parser->lexer;
    Token next = lexer_next_token(parser->lexer);
    /* Restore lexer state */
    *parser->lexer = saved;
    return next.kind;
}

/* Panic mode: skip tokens until we reach a synchronization point */
static void parser_synchronize(Parser *parser) {
    while (!parser_check(parser, TOKEN_EOF)) {
        switch (parser->current.kind) {
            case TOKEN_FUNC:
            case TOKEN_VAL:
            case TOKEN_MUT:
            case TOKEN_RETURN:
            case TOKEN_ASSERT:
            case TOKEN_IF:
            case TOKEN_WHILE:
            case TOKEN_BREAK:
            case TOKEN_CONTINUE:
            case TOKEN_RBRACE:
                return;
            default:
                parser_advance(parser);
        }
    }
}

/* ============================ Operator Precedence ========================= */

static int parser_get_precedence(TokenKind kind) {
    switch (kind) {
        case TOKEN_STAR:
        case TOKEN_SLASH:
            return 4;
        case TOKEN_PLUS:
        case TOKEN_MINUS:
            return 3;
        case TOKEN_EQUAL_EQUAL:
        case TOKEN_BANG_EQUAL:
        case TOKEN_LESS:
        case TOKEN_GREATER:
        case TOKEN_LESS_EQUAL:
        case TOKEN_GREATER_EQUAL:
            return 2;
        case TOKEN_AND_AND:
            return 1;
        case TOKEN_OR_OR:
            return 0;
        default:
            return -1;
    }
}

static BinaryOp parser_token_to_binary_op(TokenKind kind) {
    switch (kind) {
        case TOKEN_PLUS:           return OP_ADD;
        case TOKEN_MINUS:          return OP_SUB;
        case TOKEN_STAR:           return OP_MUL;
        case TOKEN_SLASH:          return OP_DIV;
        case TOKEN_EQUAL_EQUAL:    return OP_EQ;
        case TOKEN_BANG_EQUAL:     return OP_NEQ;
        case TOKEN_LESS:           return OP_LT;
        case TOKEN_GREATER:        return OP_GT;
        case TOKEN_LESS_EQUAL:     return OP_LE;
        case TOKEN_GREATER_EQUAL:  return OP_GE;
        case TOKEN_AND_AND:        return OP_AND;
        case TOKEN_OR_OR:          return OP_OR;
        default:
            fprintf(stderr, "Internal error: not a binary operator\n");
            exit(1);
    }
}

/* ============================= Expression Parsing ========================= */

static Ast *parser_parse_expression(Parser *parser);
static Ast **parser_parse_arg_list(Parser *parser, size_t *count, bool *error);

static Ast *parser_parse_primary(Parser *parser) {
    /* Handle unary minus */
    if (parser_match(parser, TOKEN_MINUS)) {
        SourceLoc loc = token_loc(&parser->previous);
        Ast *operand = parser_parse_primary(parser);
        if (!operand) return NULL;
        return ast_make_unary(OP_NEG, operand, loc);
    }

    /* Handle unary NOT */
    if (parser_match(parser, TOKEN_BANG)) {
        SourceLoc loc = token_loc(&parser->previous);
        Ast *operand = parser_parse_primary(parser);
        if (!operand) return NULL;
        return ast_make_unary(OP_NOT, operand, loc);
    }

    if (parser_match(parser, TOKEN_NUMBER)) {
        SourceLoc loc = token_loc(&parser->previous);
        char buffer[32];
        size_t len = parser->previous.length;
        if (len >= sizeof(buffer)) {
            diagnostic(ERR_P006_NUMBER_TOO_LARGE, loc.line, loc.column,
                       "Number literal too large");
            return NULL;
        }
        memcpy(buffer, parser->previous.start, len);
        buffer[len] = '\0';
        long value = strtol(buffer, NULL, 10);
        return ast_make_number(value, loc);
    }

    if (parser_match(parser, TOKEN_FLOAT)) {
        SourceLoc loc = token_loc(&parser->previous);
        char buffer[64];
        size_t len = parser->previous.length;
        if (len >= sizeof(buffer)) {
            diagnostic(ERR_P006_NUMBER_TOO_LARGE, loc.line, loc.column,
                       "Float literal too large");
            return NULL;
        }
        memcpy(buffer, parser->previous.start, len);
        buffer[len] = '\0';
        double value = strtod(buffer, NULL);
        return ast_make_float(value, loc);
    }

    if (parser_match(parser, TOKEN_TRUE)) {
        SourceLoc loc = token_loc(&parser->previous);
        return ast_make_boolean(true, loc);
    }

    if (parser_match(parser, TOKEN_FALSE)) {
        SourceLoc loc = token_loc(&parser->previous);
        return ast_make_boolean(false, loc);
    }

    if (parser_match(parser, TOKEN_IDENTIFIER)) {
        SourceLoc loc = token_loc(&parser->previous);
        const char *name_start = parser->previous.start;
        size_t name_length = parser->previous.length;

        /* Check for function call: identifier followed by '(' */
        if (parser_check(parser, TOKEN_LPAREN)) {
            parser_advance(parser);  /* consume '(' */

            size_t arg_count = 0;
            bool parse_error = false;
            Ast **arguments = parser_parse_arg_list(parser, &arg_count, &parse_error);

            if (parse_error) {
                return NULL;
            }

            if (!parser_match(parser, TOKEN_RPAREN)) {
                diagnostic(ERR_P002_EXPECTED_RPAREN, parser->current.line,
                           parser->current.column, "Expected ')' after arguments");
                free(arguments);
                return NULL;
            }

            return ast_make_func_call(name_start, name_length, arguments, arg_count, loc);
        }

        return ast_make_identifier(name_start, name_length, loc);
    }

    if (parser_match(parser, TOKEN_LPAREN)) {
        Ast *expr = parser_parse_expression(parser);
        if (!expr) return NULL;
        if (!parser_match(parser, TOKEN_RPAREN)) {
            diagnostic(ERR_P002_EXPECTED_RPAREN, parser->current.line,
                       parser->current.column, "Expected ')' after expression");
            return NULL;
        }
        return expr;
    }

    diagnostic(ERR_P001_EXPECTED_EXPRESSION, parser->current.line, parser->current.column,
               "Expected expression");
    return NULL;
}

static Ast *parser_parse_precedence(Parser *parser, int min_precedence) {
    /* Parse left operand */
    Ast *left = parser_parse_primary(parser);
    if (!left) return NULL;

    /* Consume operators while precedence is high enough */
    while (parser_get_precedence(parser->current.kind) >= min_precedence) {
        TokenKind op_kind = parser->current.kind;
        SourceLoc op_loc = token_loc(&parser->current);
        int precedence = parser_get_precedence(op_kind);
        parser_advance(parser);

        /* Parse right operand with higher precedence for left associativity */
        Ast *right = parser_parse_precedence(parser, precedence + 1);
        if (!right) return NULL;

        /* Combine into binary operation */
        left = ast_make_binary(parser_token_to_binary_op(op_kind), left, right, op_loc);
    }

    return left;
}

static Ast *parser_parse_expression(Parser *parser) {
    return parser_parse_precedence(parser, 0);
}

/* ============================= Statement Parsing ========================== */

static Ast *parser_parse_val_declaration(Parser *parser) {
    /* Already consumed TOKEN_VAL - capture its location */
    SourceLoc loc = token_loc(&parser->previous);

    /* Expect identifier */
    if (!parser_match(parser, TOKEN_IDENTIFIER)) {
        diagnostic(ERR_P003_EXPECTED_IDENTIFIER, parser->current.line,
                   parser->current.column, "Expected identifier after 'val'");
        parser_synchronize(parser);
        return NULL;
    }
    const char *name_start = parser->previous.start;
    size_t name_length = parser->previous.length;

    /* Optional ':' type annotation */
    Type type = TYPE_UNKNOWN;  /* infer from comptime expr */
    if (parser_match(parser, TOKEN_COLON)) {
        /* Expect type (i64, f64, or bool) */
        if (parser_match(parser, TOKEN_I64)) {
            type = TYPE_I64;
        } else if (parser_match(parser, TOKEN_F64)) {
            type = TYPE_F64;
        } else if (parser_match(parser, TOKEN_BOOL)) {
            type = TYPE_BOOL;
        } else {
            diagnostic(ERR_P015_EXPECTED_TYPE, parser->current.line,
                       parser->current.column, "Expected type (i64, f64, or bool)");
            parser_synchronize(parser);
            return NULL;
        }
    }

    /* Expect '=' */
    if (!parser_match(parser, TOKEN_EQUALS)) {
        diagnostic(ERR_P004_EXPECTED_EQUALS, parser->current.line,
                   parser->current.column, "Expected '=' in val declaration");
        parser_synchronize(parser);
        return NULL;
    }

    /* Parse initializer expression */
    Ast *initializer = parser_parse_expression(parser);
    if (!initializer) {
        parser_synchronize(parser);
        return NULL;
    }

    return ast_make_val_decl(name_start, name_length, type, initializer, loc);
}

static Ast *parser_parse_mut_declaration(Parser *parser) {
    /* Already consumed TOKEN_MUT - capture its location */
    SourceLoc loc = token_loc(&parser->previous);

    /* Expect identifier */
    if (!parser_match(parser, TOKEN_IDENTIFIER)) {
        diagnostic(ERR_P003_EXPECTED_IDENTIFIER, parser->current.line,
                   parser->current.column, "Expected identifier after 'mut'");
        parser_synchronize(parser);
        return NULL;
    }
    const char *name_start = parser->previous.start;
    size_t name_length = parser->previous.length;

    /* Expect ':' for type annotation */
    if (!parser_match(parser, TOKEN_COLON)) {
        diagnostic(ERR_P014_EXPECTED_COLON, parser->current.line,
                   parser->current.column, "Expected ':' after identifier");
        parser_synchronize(parser);
        return NULL;
    }

    /* Expect type (i64, f64, or bool) */
    Type type;
    if (parser_match(parser, TOKEN_I64)) {
        type = TYPE_I64;
    } else if (parser_match(parser, TOKEN_F64)) {
        type = TYPE_F64;
    } else if (parser_match(parser, TOKEN_BOOL)) {
        type = TYPE_BOOL;
    } else {
        diagnostic(ERR_P015_EXPECTED_TYPE, parser->current.line,
                   parser->current.column, "Expected type (i64, f64, or bool)");
        parser_synchronize(parser);
        return NULL;
    }

    /* Expect '=' */
    if (!parser_match(parser, TOKEN_EQUALS)) {
        diagnostic(ERR_P004_EXPECTED_EQUALS, parser->current.line,
                   parser->current.column, "Expected '=' in mut declaration");
        parser_synchronize(parser);
        return NULL;
    }

    /* Parse initializer expression */
    Ast *initializer = parser_parse_expression(parser);
    if (!initializer) {
        parser_synchronize(parser);
        return NULL;
    }

    return ast_make_mut_decl(name_start, name_length, type, initializer, loc);
}

static Ast *parser_parse_return(Parser *parser) {
    /* Already consumed TOKEN_RETURN - capture its location */
    SourceLoc loc = token_loc(&parser->previous);

    /* Check for bare return (no expression) */
    if (parser_check(parser, TOKEN_RBRACE) ||
        parser_check(parser, TOKEN_VAL) ||
        parser_check(parser, TOKEN_MUT) ||
        parser_check(parser, TOKEN_RETURN) ||
        parser_check(parser, TOKEN_ASSERT) ||
        parser_check(parser, TOKEN_IF) ||
        parser_check(parser, TOKEN_WHILE) ||
        parser_check(parser, TOKEN_BREAK) ||
        parser_check(parser, TOKEN_CONTINUE)) {
        /* Bare return for void functions */
        return ast_make_return(NULL, loc);
    }

    /* Parse return value expression */
    Ast *value = parser_parse_expression(parser);
    if (!value) {
        parser_synchronize(parser);
        return NULL;
    }

    return ast_make_return(value, loc);
}

static Ast *parser_parse_assignment(Parser *parser) {
    /* Already consumed TOKEN_IDENTIFIER - capture location and name */
    SourceLoc loc = token_loc(&parser->previous);
    const char *name_start = parser->previous.start;
    size_t name_length = parser->previous.length;

    /* Consume '=' */
    parser_advance(parser);

    /* Parse value expression */
    Ast *value = parser_parse_expression(parser);
    if (!value) {
        parser_synchronize(parser);
        return NULL;
    }

    return ast_make_assignment(name_start, name_length, value, loc);
}

static Ast *parser_parse_assert(Parser *parser) {
    /* Already consumed TOKEN_ASSERT - capture its location */
    SourceLoc loc = token_loc(&parser->previous);

    /* Parse condition expression */
    Ast *condition = parser_parse_expression(parser);
    if (!condition) {
        parser_synchronize(parser);
        return NULL;
    }

    return ast_make_assert(condition, loc);
}

/* Forward declaration for mutual recursion with parser_parse_block */
static Ast *parser_parse_statement(Parser *parser);

static Ast *parser_parse_block(Parser *parser) {
    /* LBRACE already consumed - capture its location */
    SourceLoc loc = token_loc(&parser->previous);

    Ast *block = ast_make_block(loc);

    while (!parser_check(parser, TOKEN_RBRACE) &&
           !parser_check(parser, TOKEN_EOF) &&
           !too_many_errors()) {
        Ast *statement = parser_parse_statement(parser);
        if (statement) {
            ast_block_add_statement(block, statement);
        }
    }

    if (!parser_match(parser, TOKEN_RBRACE)) {
        diagnostic(ERR_P007_EXPECTED_RBRACE, parser->current.line,
                   parser->current.column, "Expected '}' to close block");
    }

    return block;
}

static Ast *parser_parse_if(Parser *parser) {
    /* TOKEN_IF already consumed - capture its location */
    SourceLoc loc = token_loc(&parser->previous);

    /* Expect '(' */
    if (!parser_match(parser, TOKEN_LPAREN)) {
        diagnostic(ERR_P008_EXPECTED_LPAREN_IF, parser->current.line,
                   parser->current.column, "Expected '(' after 'if'");
        parser_synchronize(parser);
        return NULL;
    }

    /* Parse condition */
    Ast *condition = parser_parse_expression(parser);
    if (!condition) {
        parser_synchronize(parser);
        return NULL;
    }

    /* Expect ')' */
    if (!parser_match(parser, TOKEN_RPAREN)) {
        diagnostic(ERR_P009_EXPECTED_RPAREN_IF, parser->current.line,
                   parser->current.column, "Expected ')' after condition");
        parser_synchronize(parser);
        return NULL;
    }

    /* Expect '{' and parse block */
    if (!parser_match(parser, TOKEN_LBRACE)) {
        diagnostic(ERR_P010_EXPECTED_LBRACE_IF, parser->current.line,
                   parser->current.column, "Expected '{' for if body");
        parser_synchronize(parser);
        return NULL;
    }
    Ast *then_block = parser_parse_block(parser);

    /* Optional else */
    Ast *else_block = NULL;
    if (parser_match(parser, TOKEN_ELSE)) {
        if (!parser_match(parser, TOKEN_LBRACE)) {
            diagnostic(ERR_P010_EXPECTED_LBRACE_IF, parser->current.line,
                       parser->current.column, "Expected '{' for else body");
            parser_synchronize(parser);
            return NULL;
        }
        else_block = parser_parse_block(parser);
    }

    return ast_make_if(condition, then_block, else_block, loc);
}

static Ast *parser_parse_while(Parser *parser) {
    /* TOKEN_WHILE already consumed - capture its location */
    SourceLoc loc = token_loc(&parser->previous);

    /* Expect '(' */
    if (!parser_match(parser, TOKEN_LPAREN)) {
        diagnostic(ERR_P011_EXPECTED_LPAREN_WHILE, parser->current.line,
                   parser->current.column, "Expected '(' after 'while'");
        parser_synchronize(parser);
        return NULL;
    }

    /* Parse condition */
    Ast *condition = parser_parse_expression(parser);
    if (!condition) {
        parser_synchronize(parser);
        return NULL;
    }

    /* Expect ')' */
    if (!parser_match(parser, TOKEN_RPAREN)) {
        diagnostic(ERR_P012_EXPECTED_RPAREN_WHILE, parser->current.line,
                   parser->current.column, "Expected ')' after condition");
        parser_synchronize(parser);
        return NULL;
    }

    /* Expect '{' and parse block */
    if (!parser_match(parser, TOKEN_LBRACE)) {
        diagnostic(ERR_P013_EXPECTED_LBRACE_WHILE, parser->current.line,
                   parser->current.column, "Expected '{' for while body");
        parser_synchronize(parser);
        return NULL;
    }
    Ast *body = parser_parse_block(parser);

    return ast_make_while(condition, body, loc);
}

static Ast *parser_parse_break(Parser *parser) {
    /* TOKEN_BREAK already consumed - capture its location */
    SourceLoc loc = token_loc(&parser->previous);
    return ast_make_break(loc);
}

static Ast *parser_parse_continue(Parser *parser) {
    /* TOKEN_CONTINUE already consumed - capture its location */
    SourceLoc loc = token_loc(&parser->previous);
    return ast_make_continue(loc);
}

/* Parse a single parameter: name: type */
static bool parser_parse_parameter(Parser *parser, Parameter *param) {
    if (!parser_match(parser, TOKEN_IDENTIFIER)) {
        diagnostic(ERR_P003_EXPECTED_IDENTIFIER, parser->current.line,
                   parser->current.column, "Expected parameter name");
        return false;
    }
    param->name_start = parser->previous.start;
    param->name_length = parser->previous.length;

    if (!parser_match(parser, TOKEN_COLON)) {
        diagnostic(ERR_P014_EXPECTED_COLON, parser->current.line,
                   parser->current.column, "Expected ':' after parameter name");
        return false;
    }

    if (parser_match(parser, TOKEN_I64)) {
        param->type = TYPE_I64;
    } else if (parser_match(parser, TOKEN_F64)) {
        param->type = TYPE_F64;
    } else if (parser_match(parser, TOKEN_BOOL)) {
        param->type = TYPE_BOOL;
    } else if (parser_match(parser, TOKEN_VOID)) {
        diagnostic(ERR_S012_VOID_PARAM, parser->previous.line,
                   parser->previous.column, "void is not valid as parameter type");
        return false;
    } else {
        diagnostic(ERR_P015_EXPECTED_TYPE, parser->current.line,
                   parser->current.column, "Expected type (i64, f64, or bool)");
        return false;
    }

    return true;
}

/* Parse argument list (without parens): expr, expr, ...
 * Sets *count to number of args. Sets *error to true on parse failure.
 * Returns NULL on empty list (not an error). Caller must free the returned array. */
static Ast **parser_parse_arg_list(Parser *parser, size_t *count, bool *error) {
    *count = 0;
    *error = false;

    /* Empty argument list */
    if (parser_check(parser, TOKEN_RPAREN)) {
        return NULL;
    }

    size_t capacity = 4;
    Ast **args = malloc(capacity * sizeof(Ast *));
    if (!args) panic(ERR_I001_OUT_OF_MEMORY, "allocating argument list");

    do {
        Ast *arg = parser_parse_expression(parser);
        if (!arg) {
            free(args);
            *count = 0;
            *error = true;
            return NULL;
        }
        if (*count >= capacity) {
            capacity *= 2;
            args = realloc(args, capacity * sizeof(Ast *));
            if (!args) panic(ERR_I001_OUT_OF_MEMORY, "growing argument list");
        }
        args[(*count)++] = arg;
    } while (parser_match(parser, TOKEN_COMMA));

    return args;
}

/* Parse parameter list (without parens): name: type, name: type, ... */
static Parameter *parser_parse_param_list(Parser *parser, size_t *count) {
    *count = 0;

    /* Empty parameter list */
    if (parser_check(parser, TOKEN_RPAREN)) {
        return NULL;
    }

    size_t capacity = 4;
    Parameter *params = malloc(capacity * sizeof(Parameter));
    if (!params) {
        panic(ERR_I001_OUT_OF_MEMORY, "allocating parameters");
    }

    /* Parse first parameter */
    if (!parser_parse_parameter(parser, &params[*count])) {
        free(params);
        return NULL;
    }
    (*count)++;

    /* Parse remaining parameters */
    while (parser_match(parser, TOKEN_COMMA)) {
        if (*count >= capacity) {
            capacity *= 2;
            params = realloc(params, capacity * sizeof(Parameter));
            if (!params) {
                panic(ERR_I001_OUT_OF_MEMORY, "growing parameters");
            }
        }

        if (!parser_parse_parameter(parser, &params[*count])) {
            free(params);
            return NULL;
        }
        (*count)++;
    }

    return params;
}

/* Parse function declaration: func name(params): returnType = { body } */
static Ast *parser_parse_function(Parser *parser) {
    /* TOKEN_FUNC already consumed */
    SourceLoc loc = token_loc(&parser->previous);

    /* Expect function name */
    if (!parser_match(parser, TOKEN_IDENTIFIER)) {
        diagnostic(ERR_P017_EXPECTED_FUNC_NAME, parser->current.line,
                   parser->current.column, "Expected function name after 'func'");
        parser_synchronize(parser);
        return NULL;
    }
    const char *name_start = parser->previous.start;
    size_t name_length = parser->previous.length;

    /* Expect '(' */
    if (!parser_match(parser, TOKEN_LPAREN)) {
        diagnostic(ERR_P018_EXPECTED_LPAREN_FUNC, parser->current.line,
                   parser->current.column, "Expected '(' after function name");
        parser_synchronize(parser);
        return NULL;
    }

    /* Parse parameter list */
    size_t param_count = 0;
    Parameter *params = parser_parse_param_list(parser, &param_count);

    /* Expect ')' */
    if (!parser_match(parser, TOKEN_RPAREN)) {
        diagnostic(ERR_P019_EXPECTED_RPAREN_FUNC, parser->current.line,
                   parser->current.column, "Expected ')' after parameters");
        if (params) free(params);
        parser_synchronize(parser);
        return NULL;
    }

    /* Expect ':' */
    if (!parser_match(parser, TOKEN_COLON)) {
        diagnostic(ERR_P020_EXPECTED_COLON_FUNC, parser->current.line,
                   parser->current.column, "Expected ':' before return type");
        if (params) free(params);
        parser_synchronize(parser);
        return NULL;
    }

    /* Expect return type */
    Type return_type;
    if (parser_match(parser, TOKEN_I64)) {
        return_type = TYPE_I64;
    } else if (parser_match(parser, TOKEN_F64)) {
        return_type = TYPE_F64;
    } else if (parser_match(parser, TOKEN_BOOL)) {
        return_type = TYPE_BOOL;
    } else if (parser_match(parser, TOKEN_VOID)) {
        return_type = TYPE_VOID;
    } else {
        diagnostic(ERR_P021_EXPECTED_RETURN_TYPE, parser->current.line,
                   parser->current.column, "Expected return type (i64, f64, bool, or void)");
        if (params) free(params);
        parser_synchronize(parser);
        return NULL;
    }

    /* Expect '=' */
    if (!parser_match(parser, TOKEN_EQUALS)) {
        diagnostic(ERR_P022_EXPECTED_EQUALS_FUNC, parser->current.line,
                   parser->current.column, "Expected '=' before function body");
        if (params) free(params);
        parser_synchronize(parser);
        return NULL;
    }

    /* Expect '{' and parse block */
    if (!parser_match(parser, TOKEN_LBRACE)) {
        diagnostic(ERR_P023_EXPECTED_LBRACE_FUNC, parser->current.line,
                   parser->current.column, "Expected '{' for function body");
        if (params) free(params);
        parser_synchronize(parser);
        return NULL;
    }
    Ast *body = parser_parse_block(parser);

    return ast_make_func_decl(name_start, name_length, params, param_count,
                              return_type, body, loc);
}

static Ast *parser_parse_statement(Parser *parser) {
    if (too_many_errors()) return NULL;

    /* Block statement */
    if (parser_match(parser, TOKEN_LBRACE)) {
        return parser_parse_block(parser);
    }

    if (parser_match(parser, TOKEN_VAL)) {
        return parser_parse_val_declaration(parser);
    }

    if (parser_match(parser, TOKEN_MUT)) {
        return parser_parse_mut_declaration(parser);
    }

    if (parser_match(parser, TOKEN_RETURN)) {
        return parser_parse_return(parser);
    }

    if (parser_match(parser, TOKEN_ASSERT)) {
        return parser_parse_assert(parser);
    }

    if (parser_match(parser, TOKEN_IF)) {
        return parser_parse_if(parser);
    }

    if (parser_match(parser, TOKEN_WHILE)) {
        return parser_parse_while(parser);
    }

    if (parser_match(parser, TOKEN_BREAK)) {
        return parser_parse_break(parser);
    }

    if (parser_match(parser, TOKEN_CONTINUE)) {
        return parser_parse_continue(parser);
    }

    /* Check for assignment: identifier followed by '=' */
    if (parser_check(parser, TOKEN_IDENTIFIER) &&
        parser_peek_next(parser) == TOKEN_EQUALS) {
        parser_advance(parser);  /* consume identifier */
        return parser_parse_assignment(parser);
    }

    if (parser_check(parser, TOKEN_EOF)) {
        return NULL;
    }

    diagnostic(ERR_P005_EXPECTED_STATEMENT, parser->current.line,
               parser->current.column, "Expected statement");
    parser_synchronize(parser);
    return NULL;
}

static Ast *parser_parse_program(Parser *parser) {
    Ast *program = ast_make_program();

    while (!parser_check(parser, TOKEN_EOF) && !too_many_errors()) {
        /* Top-level must be function declarations */
        if (parser_match(parser, TOKEN_FUNC)) {
            Ast *func = parser_parse_function(parser);
            if (func) {
                ast_program_add_statement(program, func);
            }
        } else {
            diagnostic(ERR_P016_EXPECTED_FUNC, parser->current.line,
                       parser->current.column,
                       "Expected function declaration at top level");
            /* Skip tokens until we find 'func' or EOF */
            while (!parser_check(parser, TOKEN_FUNC) &&
                   !parser_check(parser, TOKEN_EOF)) {
                parser_advance(parser);
            }
        }
    }

    return program;
}

/* ============================== AST Printer =============================== */

static void parser_print_ast_step(Ast *node, int indent) {
    if (!node) return;

    for (int i = 0; i < indent; i++) {
        printf("  ");
    }

    switch (node->kind) {
        case AST_NUMBER:
            printf("NUMBER(%ld)\n", node->as.number.value);
            break;

        case AST_FLOAT:
            printf("FLOAT(%g)\n", node->as.float_lit.value);
            break;

        case AST_BOOLEAN:
            printf("BOOLEAN(%s)\n", node->as.boolean.value ? "true" : "false");
            break;

        case AST_IDENTIFIER:
            printf("IDENTIFIER(%.*s)\n",
                   (int)node->as.identifier.length,
                   node->as.identifier.start);
            break;

        case AST_BINARY: {
            const char *op_name;
            switch (node->as.binary.op) {
                case OP_ADD: op_name = "ADD"; break;
                case OP_SUB: op_name = "SUB"; break;
                case OP_MUL: op_name = "MUL"; break;
                case OP_DIV: op_name = "DIV"; break;
                case OP_EQ:  op_name = "EQ"; break;
                case OP_NEQ: op_name = "NEQ"; break;
                case OP_LT:  op_name = "LT"; break;
                case OP_GT:  op_name = "GT"; break;
                case OP_LE:  op_name = "LE"; break;
                case OP_GE:  op_name = "GE"; break;
                case OP_AND: op_name = "AND"; break;
                case OP_OR:  op_name = "OR"; break;
            }
            printf("BINARY(%s)\n", op_name);
            parser_print_ast_step(node->as.binary.left, indent + 1);
            parser_print_ast_step(node->as.binary.right, indent + 1);
            break;
        }

        case AST_UNARY: {
            const char *op_name;
            switch (node->as.unary.op) {
                case OP_NEG: op_name = "NEG"; break;
                case OP_NOT: op_name = "NOT"; break;
            }
            printf("UNARY(%s)\n", op_name);
            parser_print_ast_step(node->as.unary.operand, indent + 1);
            break;
        }

        case AST_FUNC_CALL:
            printf("FUNC_CALL(%.*s)\n",
                   (int)node->as.func_call.name_length,
                   node->as.func_call.name_start);
            for (size_t i = 0; i < node->as.func_call.arg_count; i++) {
                parser_print_ast_step(node->as.func_call.arguments[i], indent + 1);
            }
            break;

        case AST_VAL_DECL:
            printf("VAL_DECL(%.*s)\n",
                   (int)node->as.val_decl.name_length,
                   node->as.val_decl.name_start);
            parser_print_ast_step(node->as.val_decl.initializer, indent + 1);
            break;

        case AST_MUT_DECL:
            printf("MUT_DECL(%.*s)\n",
                   (int)node->as.mut_decl.name_length,
                   node->as.mut_decl.name_start);
            parser_print_ast_step(node->as.mut_decl.initializer, indent + 1);
            break;

        case AST_RETURN:
            printf("RETURN\n");
            if (node->as.return_stmt.value) {
                parser_print_ast_step(node->as.return_stmt.value, indent + 1);
            }
            break;

        case AST_ASSIGNMENT:
            printf("ASSIGNMENT(%.*s)\n",
                   (int)node->as.assignment.name_length,
                   node->as.assignment.name_start);
            parser_print_ast_step(node->as.assignment.value, indent + 1);
            break;

        case AST_ASSERT:
            printf("ASSERT\n");
            parser_print_ast_step(node->as.assert_stmt.condition, indent + 1);
            break;

        case AST_IF:
            printf("IF\n");
            for (int i = 0; i < indent + 1; i++) printf("  ");
            printf("COND:\n");
            parser_print_ast_step(node->as.if_stmt.condition, indent + 2);
            for (int i = 0; i < indent + 1; i++) printf("  ");
            printf("THEN:\n");
            parser_print_ast_step(node->as.if_stmt.then_block, indent + 2);
            if (node->as.if_stmt.else_block) {
                for (int i = 0; i < indent + 1; i++) printf("  ");
                printf("ELSE:\n");
                parser_print_ast_step(node->as.if_stmt.else_block, indent + 2);
            }
            break;

        case AST_WHILE:
            printf("WHILE\n");
            for (int i = 0; i < indent + 1; i++) printf("  ");
            printf("COND:\n");
            parser_print_ast_step(node->as.while_stmt.condition, indent + 2);
            for (int i = 0; i < indent + 1; i++) printf("  ");
            printf("BODY:\n");
            parser_print_ast_step(node->as.while_stmt.body, indent + 2);
            break;

        case AST_BREAK:
            printf("BREAK\n");
            break;

        case AST_CONTINUE:
            printf("CONTINUE\n");
            break;

        case AST_BLOCK:
            printf("BLOCK\n");
            for (size_t i = 0; i < node->as.block.count; i++) {
                parser_print_ast_step(node->as.block.statements[i], indent + 1);
            }
            break;

        case AST_FUNC_DECL:
            printf("FUNC_DECL(%.*s) -> %s\n",
                   (int)node->as.func_decl.name_length,
                   node->as.func_decl.name_start,
                   type_name(node->as.func_decl.return_type));
            for (size_t i = 0; i < node->as.func_decl.param_count; i++) {
                for (int j = 0; j < indent + 1; j++) printf("  ");
                Parameter *p = &node->as.func_decl.params[i];
                printf("PARAM(%.*s: %s)\n",
                       (int)p->name_length, p->name_start,
                       type_name(p->type));
            }
            for (int i = 0; i < indent + 1; i++) printf("  ");
            printf("BODY:\n");
            parser_print_ast_step(node->as.func_decl.body, indent + 2);
            break;

        case AST_PROGRAM:
            printf("PROGRAM\n");
            for (size_t i = 0; i < node->as.program.count; i++) {
                parser_print_ast_step(node->as.program.statements[i], indent + 1);
            }
            break;
    }
}

static void parser_print_ast(Ast *ast) {
    printf("=== AST ===\n");
    parser_print_ast_step(ast, 0);
    printf("\n");
}

/* ============================= Scope Management =========================== */

/* Variable and Scope structs (defined here for use in folding) */
struct Variable {
    const char *name_start;
    size_t name_length;
    bool is_mutable;
    Type type;
    bool is_comptime;
    union {
        long int_value;
        double float_value;
    } comptime_value;
};
typedef struct Variable Variable;

struct Scope {
    Variable *vars;
    size_t count;
    size_t capacity;
    struct Scope *parent;
    int loop_depth;
};
typedef struct Scope Scope;

static Scope *scope_create(Scope *parent) {
    Scope *scope = malloc(sizeof(Scope));
    if (!scope) {
        panic(ERR_I001_OUT_OF_MEMORY, "allocating scope");
    }
    scope->vars = malloc(8 * sizeof(Variable));
    if (!scope->vars) {
        free(scope);
        panic(ERR_I001_OUT_OF_MEMORY, "allocating scope variables");
    }
    scope->count = 0;
    scope->capacity = 8;
    scope->parent = parent;
    scope->loop_depth = parent ? parent->loop_depth : 0;
    return scope;
}

static void scope_destroy(Scope *scope) {
    free(scope->vars);
    free(scope);
}

/* Lookup in current scope only (for duplicate detection) */
static Variable *scope_lookup_local(Scope *scope, const char *name_start,
                                     size_t name_length) {
    for (size_t i = 0; i < scope->count; i++) {
        if (scope->vars[i].name_length == name_length &&
            memcmp(scope->vars[i].name_start, name_start, name_length) == 0) {
            return &scope->vars[i];
        }
    }
    return NULL;
}

/* Lookup walking up the scope chain (for variable resolution) */
static Variable *scope_lookup(Scope *scope, const char *name_start,
                               size_t name_length) {
    while (scope != NULL) {
        Variable *v = scope_lookup_local(scope, name_start, name_length);
        if (v != NULL) {
            return v;
        }
        scope = scope->parent;
    }
    return NULL;
}

static Variable *scope_add(Scope *scope, const char *name_start,
                           size_t name_length, bool is_mutable, Type type,
                           SourceLoc loc) {
    /* Check for duplicates in current scope only */
    if (scope_lookup_local(scope, name_start, name_length) != NULL) {
        diagnostic(ERR_S001_DUPLICATE_VARIABLE, loc.line, loc.column,
                   "Variable '%.*s' already declared in this scope",
                   (int)name_length, name_start);
        return NULL;
    }

    /* Grow if needed */
    if (scope->count >= scope->capacity) {
        scope->capacity *= 2;
        scope->vars = realloc(scope->vars, scope->capacity * sizeof(Variable));
        if (!scope->vars) {
            panic(ERR_I001_OUT_OF_MEMORY, "growing scope variables");
        }
    }

    /* Add variable */
    Variable *v = &scope->vars[scope->count];
    v->name_start = name_start;
    v->name_length = name_length;
    v->is_mutable = is_mutable;
    v->type = type;
    v->is_comptime = false;
    scope->count++;
    return v;
}

static void scope_add_comptime_int(Scope *scope, const char *name_start,
                                   size_t name_length, long value,
                                   SourceLoc loc) {
    Variable *v = scope_add(scope, name_start, name_length, false,
                            TYPE_COMPTIME_INT, loc);
    if (v != NULL) {
        v->is_comptime = true;
        v->comptime_value.int_value = value;
    }
}

static void scope_add_comptime_float(Scope *scope, const char *name_start,
                                     size_t name_length, double value,
                                     SourceLoc loc) {
    Variable *v = scope_add(scope, name_start, name_length, false,
                            TYPE_COMPTIME_FLOAT, loc);
    if (v != NULL) {
        v->is_comptime = true;
        v->comptime_value.float_value = value;
    }
}

/* ========================== Function Table ========================== */

typedef struct {
    const char *name_start;
    size_t name_length;
    Type return_type;
    Type *param_types;
    size_t param_count;
    SourceLoc loc;
} FunctionEntry;

typedef struct {
    FunctionEntry *functions;
    size_t count;
    size_t capacity;
} FunctionTable;

static FunctionTable *func_table_create(void) {
    FunctionTable *table = malloc(sizeof(FunctionTable));
    if (!table) panic(ERR_I001_OUT_OF_MEMORY, "allocating function table");
    table->functions = malloc(8 * sizeof(FunctionEntry));
    if (!table->functions) {
        free(table);
        panic(ERR_I001_OUT_OF_MEMORY, "allocating function entries");
    }
    table->count = 0;
    table->capacity = 8;
    return table;
}

static void func_table_destroy(FunctionTable *table) {
    for (size_t i = 0; i < table->count; i++) {
        free(table->functions[i].param_types);
    }
    free(table->functions);
    free(table);
}

static FunctionEntry *func_table_lookup(FunctionTable *table,
                                        const char *name_start, size_t name_length) {
    for (size_t i = 0; i < table->count; i++) {
        if (table->functions[i].name_length == name_length &&
            memcmp(table->functions[i].name_start, name_start, name_length) == 0) {
            return &table->functions[i];
        }
    }
    return NULL;
}

static void func_table_add(FunctionTable *table, Ast *func_decl) {
    const char *name_start = func_decl->as.func_decl.name_start;
    size_t name_length = func_decl->as.func_decl.name_length;

    /* Check for duplicates */
    if (func_table_lookup(table, name_start, name_length) != NULL) {
        diagnostic(ERR_S008_DUPLICATE_FUNCTION, func_decl->loc.line,
                   func_decl->loc.column, "Duplicate function '%.*s'",
                   (int)name_length, name_start);
        return;
    }

    /* Grow if needed */
    if (table->count >= table->capacity) {
        table->capacity *= 2;
        table->functions = realloc(table->functions,
                                   table->capacity * sizeof(FunctionEntry));
        if (!table->functions) panic(ERR_I001_OUT_OF_MEMORY, "growing function table");
    }

    FunctionEntry *entry = &table->functions[table->count++];
    entry->name_start = name_start;
    entry->name_length = name_length;
    entry->return_type = func_decl->as.func_decl.return_type;
    entry->loc = func_decl->loc;

    /* Store parameter types */
    size_t param_count = func_decl->as.func_decl.param_count;
    entry->param_count = param_count;
    if (param_count > 0) {
        entry->param_types = malloc(param_count * sizeof(Type));
        if (!entry->param_types) panic(ERR_I001_OUT_OF_MEMORY, "allocating param types");
        for (size_t i = 0; i < param_count; i++) {
            entry->param_types[i] = func_decl->as.func_decl.params[i].type;
        }
    } else {
        entry->param_types = NULL;
    }
}

/* ============================= Constant Folding ============================ */

/* Returns true if node is a compile-time constant (literal or comptime variable) */
static bool is_comptime_constant(Ast *node, Scope *scope) {
    if (node->kind == AST_NUMBER || node->kind == AST_FLOAT ||
        node->kind == AST_BOOLEAN) {
        return true;
    }
    if (node->kind == AST_IDENTIFIER && scope != NULL) {
        Variable *v = scope_lookup(scope, node->as.identifier.start,
                                   node->as.identifier.length);
        return v != NULL && v->is_comptime;
    }
    return false;
}

/* Get integer value from comptime node */
static long get_comptime_int(Ast *node, Scope *scope) {
    if (node->kind == AST_NUMBER) return node->as.number.value;
    if (node->kind == AST_FLOAT) return (long)node->as.float_lit.value;
    if (node->kind == AST_IDENTIFIER && scope != NULL) {
        Variable *v = scope_lookup(scope, node->as.identifier.start,
                                   node->as.identifier.length);
        if (v != NULL && v->is_comptime) {
            return v->comptime_value.int_value;
        }
    }
    return 0;
}

/* Get float value from comptime node */
static double get_comptime_float(Ast *node, Scope *scope) {
    if (node->kind == AST_FLOAT) return node->as.float_lit.value;
    if (node->kind == AST_NUMBER) return (double)node->as.number.value;
    if (node->kind == AST_IDENTIFIER && scope != NULL) {
        Variable *v = scope_lookup(scope, node->as.identifier.start,
                                   node->as.identifier.length);
        if (v != NULL && v->is_comptime) {
            if (v->type == TYPE_COMPTIME_FLOAT) {
                return v->comptime_value.float_value;
            } else {
                return (double)v->comptime_value.int_value;
            }
        }
    }
    return 0.0;
}

/* Check if node evaluates to int (for determining result type) */
static bool is_comptime_int_type(Ast *node, Scope *scope) {
    if (node->kind == AST_NUMBER) return true;
    if (node->kind == AST_FLOAT) return false;
    if (node->kind == AST_IDENTIFIER && scope != NULL) {
        Variable *v = scope_lookup(scope, node->as.identifier.start,
                                   node->as.identifier.length);
        if (v != NULL && v->is_comptime) {
            return v->type == TYPE_COMPTIME_INT;
        }
    }
    return true;  /* default to int */
}

/* Try to fold a binary arithmetic operation. Returns folded node or NULL.
 * Sets *div_by_zero to true if division by zero detected. */
static Ast *try_fold_binary(Ast *node, Scope *scope, bool *div_by_zero) {
    *div_by_zero = false;

    Ast *left = node->as.binary.left;
    Ast *right = node->as.binary.right;

    if (!is_comptime_constant(left, scope) || !is_comptime_constant(right, scope)) {
        return NULL;
    }

    BinaryOp op = node->as.binary.op;

    /* Only fold arithmetic operations */
    if (op != OP_ADD && op != OP_SUB && op != OP_MUL && op != OP_DIV) {
        return NULL;
    }

    /* Check for division by zero */
    if (op == OP_DIV) {
        long r_int = get_comptime_int(right, scope);
        double r_float = get_comptime_float(right, scope);
        if (r_int == 0 && r_float == 0.0) {
            *div_by_zero = true;
            return NULL;
        }
    }

    /* Determine if result is int or float */
    bool result_is_float = !is_comptime_int_type(left, scope) ||
                           !is_comptime_int_type(right, scope);

    if (result_is_float) {
        double l = get_comptime_float(left, scope);
        double r = get_comptime_float(right, scope);
        double result;

        switch (op) {
            case OP_ADD: result = l + r; break;
            case OP_SUB: result = l - r; break;
            case OP_MUL: result = l * r; break;
            case OP_DIV: result = l / r; break;
            default: return NULL;
        }

        Ast *folded = ast_make_float(result, node->loc);
        folded->expr_type = TYPE_COMPTIME_FLOAT;
        return folded;
    } else {
        long l = get_comptime_int(left, scope);
        long r = get_comptime_int(right, scope);
        long result;

        switch (op) {
            case OP_ADD: result = l + r; break;
            case OP_SUB: result = l - r; break;
            case OP_MUL: result = l * r; break;
            case OP_DIV: result = l / r; break;
            default: return NULL;
        }

        Ast *folded = ast_make_number(result, node->loc);
        folded->expr_type = TYPE_COMPTIME_INT;
        return folded;
    }
}

/* Try to fold a comparison operation. Returns folded bool node or NULL. */
static Ast *try_fold_comparison(Ast *node, Scope *scope) {
    Ast *left = node->as.binary.left;
    Ast *right = node->as.binary.right;

    if (!is_comptime_constant(left, scope) || !is_comptime_constant(right, scope)) {
        return NULL;
    }

    BinaryOp op = node->as.binary.op;
    if (op < OP_EQ || op > OP_GE) {
        return NULL;
    }

    bool result_is_float = !is_comptime_int_type(left, scope) ||
                           !is_comptime_int_type(right, scope);
    bool result;

    if (result_is_float) {
        double l = get_comptime_float(left, scope);
        double r = get_comptime_float(right, scope);
        switch (op) {
            case OP_EQ:  result = l == r; break;
            case OP_NEQ: result = l != r; break;
            case OP_LT:  result = l < r; break;
            case OP_GT:  result = l > r; break;
            case OP_LE:  result = l <= r; break;
            case OP_GE:  result = l >= r; break;
            default: return NULL;
        }
    } else {
        long l = get_comptime_int(left, scope);
        long r = get_comptime_int(right, scope);
        switch (op) {
            case OP_EQ:  result = l == r; break;
            case OP_NEQ: result = l != r; break;
            case OP_LT:  result = l < r; break;
            case OP_GT:  result = l > r; break;
            case OP_LE:  result = l <= r; break;
            case OP_GE:  result = l >= r; break;
            default: return NULL;
        }
    }

    Ast *folded = ast_make_boolean(result, node->loc);
    folded->expr_type = TYPE_BOOL;
    return folded;
}

/* Try to fold a unary operation. Returns folded node or NULL. */
static Ast *try_fold_unary(Ast *node, Scope *scope) {
    Ast *operand = node->as.unary.operand;

    if (!is_comptime_constant(operand, scope)) return NULL;

    if (node->as.unary.op == OP_NEG) {
        if (is_comptime_int_type(operand, scope)) {
            Ast *folded = ast_make_number(-get_comptime_int(operand, scope), node->loc);
            folded->expr_type = TYPE_COMPTIME_INT;
            return folded;
        } else {
            Ast *folded = ast_make_float(-get_comptime_float(operand, scope), node->loc);
            folded->expr_type = TYPE_COMPTIME_FLOAT;
            return folded;
        }
    }

    if (node->as.unary.op == OP_NOT && operand->kind == AST_BOOLEAN) {
        Ast *folded = ast_make_boolean(!operand->as.boolean.value, node->loc);
        folded->expr_type = TYPE_BOOL;
        return folded;
    }

    return NULL;
}

/* ============================== Type Checking ============================== */

static Type typecheck_expression(Ast *node, Scope *scope, FunctionTable *func_table);
static void typecheck_statement(Ast *node, Scope **scope, Type return_type, FunctionTable *func_table);

static Type typecheck_expression(Ast *node, Scope *scope, FunctionTable *func_table) {
    switch (node->kind) {
        case AST_NUMBER:
            node->expr_type = TYPE_COMPTIME_INT;
            return TYPE_COMPTIME_INT;

        case AST_FLOAT:
            node->expr_type = TYPE_COMPTIME_FLOAT;
            return TYPE_COMPTIME_FLOAT;

        case AST_BOOLEAN:
            node->expr_type = TYPE_BOOL;
            return TYPE_BOOL;

        case AST_IDENTIFIER: {
            Variable *v = scope_lookup(scope, node->as.identifier.start,
                                        node->as.identifier.length);
            if (v == NULL) {
                diagnostic(ERR_S002_UNDECLARED_VARIABLE, node->loc.line,
                           node->loc.column, "Undeclared variable '%.*s'",
                           (int)node->as.identifier.length,
                           node->as.identifier.start);
                node->expr_type = TYPE_I64;  /* Default to prevent cascading */
                return TYPE_I64;
            }
            node->expr_type = v->type;
            return v->type;
        }

        case AST_BINARY: {
            Type left_type = typecheck_expression(node->as.binary.left, scope, func_table);
            Type right_type = typecheck_expression(node->as.binary.right, scope, func_table);

            switch (node->as.binary.op) {
                /* Arithmetic: numeric x numeric -> numeric */
                case OP_ADD:
                case OP_SUB:
                case OP_MUL:
                case OP_DIV: {
                    Type result = resolve_numeric_binary_type(left_type, right_type);
                    if (result == TYPE_VOID) {
                        diagnostic(ERR_S006_TYPE_MISMATCH, node->loc.line, node->loc.column,
                                   "Cannot mix %s and %s in arithmetic operation",
                                   type_name(left_type), type_name(right_type));
                        node->expr_type = TYPE_I64;  /* default */
                        return TYPE_I64;
                    }

                    /* Try constant folding */
                    bool div_by_zero = false;
                    Ast *folded = try_fold_binary(node, scope, &div_by_zero);
                    if (div_by_zero) {
                        diagnostic(ERR_S019_DIVISION_BY_ZERO, node->loc.line, node->loc.column,
                                   "Division by zero in constant expression");
                        node->expr_type = result;
                        return result;
                    }
                    if (folded) {
                        /* Replace node with folded result */
                        ast_free(node->as.binary.left);
                        ast_free(node->as.binary.right);
                        *node = *folded;
                        free(folded);
                        return node->expr_type;
                    }

                    node->expr_type = result;
                    return result;
                }

                /* Comparison: numeric x numeric -> bool (same concrete type required) */
                case OP_EQ:
                case OP_NEQ:
                case OP_LT:
                case OP_GT:
                case OP_LE:
                case OP_GE: {
                    Type result = resolve_numeric_binary_type(left_type, right_type);
                    if (result == TYPE_VOID) {
                        diagnostic(ERR_S006_TYPE_MISMATCH, node->loc.line, node->loc.column,
                                   "Cannot compare %s and %s",
                                   type_name(left_type), type_name(right_type));
                    }

                    /* Try constant folding */
                    Ast *folded = try_fold_comparison(node, scope);
                    if (folded) {
                        ast_free(node->as.binary.left);
                        ast_free(node->as.binary.right);
                        *node = *folded;
                        free(folded);
                        return TYPE_BOOL;
                    }

                    node->expr_type = TYPE_BOOL;
                    return TYPE_BOOL;
                }

                /* Logical: bool x bool -> bool */
                case OP_AND:
                case OP_OR:
                    if (left_type != TYPE_BOOL) {
                        diagnostic(ERR_S006_TYPE_MISMATCH, node->as.binary.left->loc.line,
                                   node->as.binary.left->loc.column,
                                   "Expected bool, got %s", type_name(left_type));
                    }
                    if (right_type != TYPE_BOOL) {
                        diagnostic(ERR_S006_TYPE_MISMATCH, node->as.binary.right->loc.line,
                                   node->as.binary.right->loc.column,
                                   "Expected bool, got %s", type_name(right_type));
                    }
                    node->expr_type = TYPE_BOOL;
                    return TYPE_BOOL;
            }
            break;
        }

        case AST_UNARY: {
            Type operand_type = typecheck_expression(node->as.unary.operand, scope, func_table);

            /* Try constant folding first */
            Ast *folded = try_fold_unary(node, scope);
            if (folded) {
                ast_free(node->as.unary.operand);
                *node = *folded;
                free(folded);
                return node->expr_type;
            }

            switch (node->as.unary.op) {
                /* Negation: numeric -> numeric (preserves type) */
                case OP_NEG:
                    if (!type_is_numeric(operand_type)) {
                        diagnostic(ERR_S006_TYPE_MISMATCH, node->as.unary.operand->loc.line,
                                   node->as.unary.operand->loc.column,
                                   "Expected numeric type for negation, got %s",
                                   type_name(operand_type));
                        node->expr_type = TYPE_I64;
                        return TYPE_I64;
                    }
                    node->expr_type = operand_type;
                    return operand_type;

                /* NOT: bool -> bool */
                case OP_NOT:
                    if (operand_type != TYPE_BOOL) {
                        diagnostic(ERR_S006_TYPE_MISMATCH, node->as.unary.operand->loc.line,
                                   node->as.unary.operand->loc.column,
                                   "Expected bool for logical NOT, got %s",
                                   type_name(operand_type));
                    }
                    node->expr_type = TYPE_BOOL;
                    return TYPE_BOOL;
            }
            break;
        }

        case AST_FUNC_CALL: {
            const char *name_start = node->as.func_call.name_start;
            size_t name_length = node->as.func_call.name_length;

            /* Look up function in table */
            FunctionEntry *entry = func_table_lookup(func_table, name_start, name_length);
            if (entry == NULL) {
                diagnostic(ERR_S016_UNDEFINED_FUNCTION, node->loc.line,
                           node->loc.column, "Undefined function '%.*s'",
                           (int)name_length, name_start);
                node->expr_type = TYPE_I64;  /* Default to prevent cascading */
                return TYPE_I64;
            }

            /* Check argument count */
            if (node->as.func_call.arg_count != entry->param_count) {
                diagnostic(ERR_S017_WRONG_ARG_COUNT, node->loc.line,
                           node->loc.column,
                           "Function '%.*s' expects %zu arguments, got %zu",
                           (int)name_length, name_start,
                           entry->param_count, node->as.func_call.arg_count);
            }

            /* Type check each argument */
            size_t check_count = node->as.func_call.arg_count < entry->param_count
                                 ? node->as.func_call.arg_count : entry->param_count;
            for (size_t i = 0; i < check_count; i++) {
                Type arg_type = typecheck_expression(node->as.func_call.arguments[i],
                                                     scope, func_table);
                if (!type_can_coerce(arg_type, entry->param_types[i])) {
                    diagnostic(ERR_S018_ARG_TYPE_MISMATCH,
                               node->as.func_call.arguments[i]->loc.line,
                               node->as.func_call.arguments[i]->loc.column,
                               "Argument %zu: expected %s, got %s",
                               i + 1, type_name(entry->param_types[i]),
                               type_name(arg_type));
                }
            }

            /* Also type-check remaining arguments (even if wrong count) */
            for (size_t i = check_count; i < node->as.func_call.arg_count; i++) {
                typecheck_expression(node->as.func_call.arguments[i], scope, func_table);
            }

            node->expr_type = entry->return_type;
            return entry->return_type;
        }

        default:
            break;
    }

    node->expr_type = TYPE_I64;  /* Default */
    return TYPE_I64;
}

static void typecheck_statement(Ast *node, Scope **scope, Type return_type, FunctionTable *func_table) {
    switch (node->kind) {
        case AST_VAL_DECL: {
            Type init_type = typecheck_expression(node->as.val_decl.initializer, *scope, func_table);
            Type declared = node->as.val_decl.type;

            if (declared == TYPE_UNKNOWN) {
                /* No explicit type — must be comptime */
                if (!type_is_comptime(init_type)) {
                    diagnostic(ERR_S020_TYPE_REQUIRED, node->loc.line, node->loc.column,
                               "Type annotation required (expression is not compile-time constant)");
                    declared = TYPE_I64;  /* fallback */
                } else {
                    declared = init_type;  /* inherit comptime type */
                }
                node->as.val_decl.type = declared;  /* update AST */
            } else {
                /* Explicit type — check coercion */
                if (!type_can_coerce(init_type, declared)) {
                    diagnostic(ERR_S006_TYPE_MISMATCH, node->as.val_decl.initializer->loc.line,
                               node->as.val_decl.initializer->loc.column,
                               "Cannot assign %s to variable of type %s",
                               type_name(init_type), type_name(declared));
                }
            }

            /* Add to scope with comptime info if applicable */
            if (type_is_comptime(declared) && is_comptime_constant(node->as.val_decl.initializer, *scope)) {
                if (declared == TYPE_COMPTIME_INT) {
                    scope_add_comptime_int(*scope, node->as.val_decl.name_start,
                                           node->as.val_decl.name_length,
                                           get_comptime_int(node->as.val_decl.initializer, *scope),
                                           node->loc);
                } else {
                    scope_add_comptime_float(*scope, node->as.val_decl.name_start,
                                             node->as.val_decl.name_length,
                                             get_comptime_float(node->as.val_decl.initializer, *scope),
                                             node->loc);
                }
            } else {
                scope_add(*scope, node->as.val_decl.name_start,
                          node->as.val_decl.name_length, false, declared, node->loc);
            }
            break;
        }

        case AST_MUT_DECL: {
            Type init_type = typecheck_expression(node->as.mut_decl.initializer, *scope, func_table);
            Type declared = node->as.mut_decl.type;

            if (!type_can_coerce(init_type, declared)) {
                diagnostic(ERR_S006_TYPE_MISMATCH, node->as.mut_decl.initializer->loc.line,
                           node->as.mut_decl.initializer->loc.column,
                           "Cannot assign %s to variable of type %s",
                           type_name(init_type), type_name(declared));
            }

            scope_add(*scope, node->as.mut_decl.name_start,
                      node->as.mut_decl.name_length, true, declared, node->loc);
            break;
        }

        case AST_ASSIGNMENT: {
            Variable *v = scope_lookup(*scope, node->as.assignment.name_start,
                                        node->as.assignment.name_length);
            if (v == NULL) {
                diagnostic(ERR_S002_UNDECLARED_VARIABLE, node->loc.line,
                           node->loc.column, "Undeclared variable '%.*s'",
                           (int)node->as.assignment.name_length,
                           node->as.assignment.name_start);
                break;
            }
            if (!v->is_mutable) {
                diagnostic(ERR_S003_IMMUTABLE_ASSIGNMENT, node->loc.line,
                           node->loc.column,
                           "Cannot assign to immutable variable '%.*s'",
                           (int)node->as.assignment.name_length,
                           node->as.assignment.name_start);
            }

            Type value_type = typecheck_expression(node->as.assignment.value, *scope, func_table);
            if (!type_can_coerce(value_type, v->type)) {
                diagnostic(ERR_S006_TYPE_MISMATCH, node->as.assignment.value->loc.line,
                           node->as.assignment.value->loc.column,
                           "Cannot assign %s to variable of type %s",
                           type_name(value_type), type_name(v->type));
            }
            break;
        }

        case AST_RETURN: {
            Ast *value = node->as.return_stmt.value;

            if (return_type == TYPE_VOID) {
                /* Void function should not return a value */
                if (value != NULL) {
                    diagnostic(ERR_S014_VOID_RETURN_VALUE, node->loc.line,
                               node->loc.column,
                               "Void function cannot return a value");
                }
            } else {
                /* Non-void function must return a value */
                if (value == NULL) {
                    diagnostic(ERR_S015_MISSING_RETURN_VALUE, node->loc.line,
                               node->loc.column,
                               "Function must return a value of type %s",
                               type_name(return_type));
                } else {
                    Type value_type = typecheck_expression(value, *scope, func_table);
                    if (!type_can_coerce(value_type, return_type)) {
                        diagnostic(ERR_S013_RETURN_TYPE_MISMATCH, node->loc.line,
                                   node->loc.column,
                                   "Return type mismatch: expected %s, got %s",
                                   type_name(return_type), type_name(value_type));
                    }
                }
            }
            break;
        }

        case AST_ASSERT: {
            Type cond_type = typecheck_expression(node->as.assert_stmt.condition, *scope, func_table);
            if (cond_type != TYPE_BOOL) {
                diagnostic(ERR_S007_CONDITION_NOT_BOOL, node->loc.line,
                           node->loc.column,
                           "Assert condition must be bool, got %s",
                           type_name(cond_type));
            }
            break;
        }

        case AST_IF: {
            Type cond_type = typecheck_expression(node->as.if_stmt.condition, *scope, func_table);
            if (cond_type != TYPE_BOOL) {
                diagnostic(ERR_S007_CONDITION_NOT_BOOL, node->loc.line,
                           node->loc.column,
                           "If condition must be bool, got %s",
                           type_name(cond_type));
            }

            /* Typecheck then block */
            Ast *then_block = node->as.if_stmt.then_block;
            Scope *then_scope = scope_create(*scope);
            for (size_t i = 0; i < then_block->as.block.count; i++) {
                typecheck_statement(then_block->as.block.statements[i], &then_scope, return_type, func_table);
            }
            scope_destroy(then_scope);

            /* Typecheck else block if present */
            if (node->as.if_stmt.else_block) {
                Ast *else_block = node->as.if_stmt.else_block;
                Scope *else_scope = scope_create(*scope);
                for (size_t i = 0; i < else_block->as.block.count; i++) {
                    typecheck_statement(else_block->as.block.statements[i], &else_scope, return_type, func_table);
                }
                scope_destroy(else_scope);
            }
            break;
        }

        case AST_WHILE: {
            Type cond_type = typecheck_expression(node->as.while_stmt.condition, *scope, func_table);
            if (cond_type != TYPE_BOOL) {
                diagnostic(ERR_S007_CONDITION_NOT_BOOL, node->loc.line,
                           node->loc.column,
                           "While condition must be bool, got %s",
                           type_name(cond_type));
            }

            /* Typecheck body */
            Ast *body = node->as.while_stmt.body;
            Scope *body_scope = scope_create(*scope);
            body_scope->loop_depth = (*scope)->loop_depth + 1;
            for (size_t i = 0; i < body->as.block.count; i++) {
                typecheck_statement(body->as.block.statements[i], &body_scope, return_type, func_table);
            }
            scope_destroy(body_scope);
            break;
        }

        case AST_BREAK:
            if ((*scope)->loop_depth == 0) {
                diagnostic(ERR_S004_BREAK_OUTSIDE_LOOP, node->loc.line,
                           node->loc.column, "'break' outside of loop");
            }
            break;

        case AST_CONTINUE:
            if ((*scope)->loop_depth == 0) {
                diagnostic(ERR_S005_CONTINUE_OUTSIDE_LOOP, node->loc.line,
                           node->loc.column, "'continue' outside of loop");
            }
            break;

        case AST_BLOCK: {
            Scope *block_scope = scope_create(*scope);
            for (size_t i = 0; i < node->as.block.count; i++) {
                typecheck_statement(node->as.block.statements[i], &block_scope, return_type, func_table);
            }
            scope_destroy(block_scope);
            break;
        }

        default:
            break;
    }
}

/* Type check a function declaration */
static void typecheck_function(Ast *func_decl, FunctionTable *func_table) {
    /* Create scope with parameters */
    Scope *scope = scope_create(NULL);

    /* Add parameters to scope, check for duplicates */
    for (size_t i = 0; i < func_decl->as.func_decl.param_count; i++) {
        Parameter *param = &func_decl->as.func_decl.params[i];

        /* Check for duplicate parameter names */
        if (scope_lookup_local(scope, param->name_start, param->name_length)) {
            diagnostic(ERR_S011_DUPLICATE_PARAM, func_decl->loc.line,
                       func_decl->loc.column, "Duplicate parameter '%.*s'",
                       (int)param->name_length, param->name_start);
        } else {
            scope_add(scope, param->name_start, param->name_length,
                      false, param->type, func_decl->loc);  /* Parameters are immutable */
        }
    }

    /* Type check body statements with the function's return type */
    Type return_type = func_decl->as.func_decl.return_type;
    Ast *body = func_decl->as.func_decl.body;
    for (size_t i = 0; i < body->as.block.count; i++) {
        typecheck_statement(body->as.block.statements[i], &scope, return_type, func_table);
    }

    scope_destroy(scope);
}

static void typecheck_program(Ast *program) {
    FunctionTable *func_table = func_table_create();
    bool has_main = false;

    /* First pass: register all functions */
    for (size_t i = 0; i < program->as.program.count; i++) {
        Ast *node = program->as.program.statements[i];
        if (node->kind == AST_FUNC_DECL) {
            func_table_add(func_table, node);

            /* Check if this is main */
            if (node->as.func_decl.name_length == 4 &&
                memcmp(node->as.func_decl.name_start, "main", 4) == 0) {
                has_main = true;

                /* Verify main signature: no parameters, void return */
                if (node->as.func_decl.param_count != 0) {
                    diagnostic(ERR_S010_INVALID_MAIN_SIG, node->loc.line,
                               node->loc.column, "main must have no parameters");
                }
                if (node->as.func_decl.return_type != TYPE_VOID) {
                    diagnostic(ERR_S010_INVALID_MAIN_SIG, node->loc.line,
                               node->loc.column, "main must return void");
                }
            }
        }
    }

    /* Check for main function */
    if (!has_main) {
        diagnostic(ERR_S009_MISSING_MAIN, 1, 1, "No 'main' function defined");
    }

    /* Second pass: type check each function */
    for (size_t i = 0; i < program->as.program.count; i++) {
        Ast *node = program->as.program.statements[i];
        if (node->kind == AST_FUNC_DECL) {
            typecheck_function(node, func_table);
        }
    }

    func_table_destroy(func_table);
}

/* ============================ Code Generation ============================= */

static void codegen_emit_expression(FILE *out, Ast *node);

static void codegen_emit_expression(FILE *out, Ast *node) {
    switch (node->kind) {
        case AST_NUMBER:
            fprintf(out, "%ldL", node->as.number.value);
            break;

        case AST_FLOAT:
            fprintf(out, "%g", node->as.float_lit.value);
            break;

        case AST_BOOLEAN:
            fprintf(out, "%d", node->as.boolean.value ? 1 : 0);
            break;

        case AST_IDENTIFIER:
            fprintf(out, "%.*s", (int)node->as.identifier.length,
                    node->as.identifier.start);
            break;

        case AST_BINARY: {
            fprintf(out, "(");
            codegen_emit_expression(out, node->as.binary.left);

            switch (node->as.binary.op) {
                case OP_ADD: fprintf(out, " + "); break;
                case OP_SUB: fprintf(out, " - "); break;
                case OP_MUL: fprintf(out, " * "); break;
                case OP_DIV: fprintf(out, " / "); break;
                case OP_EQ:  fprintf(out, " == "); break;
                case OP_NEQ: fprintf(out, " != "); break;
                case OP_LT:  fprintf(out, " < "); break;
                case OP_GT:  fprintf(out, " > "); break;
                case OP_LE:  fprintf(out, " <= "); break;
                case OP_GE:  fprintf(out, " >= "); break;
                case OP_AND: fprintf(out, " && "); break;
                case OP_OR:  fprintf(out, " || "); break;
            }

            codegen_emit_expression(out, node->as.binary.right);
            fprintf(out, ")");
            break;
        }

        case AST_UNARY: {
            fprintf(out, "(");
            switch (node->as.unary.op) {
                case OP_NEG: fprintf(out, "-"); break;
                case OP_NOT: fprintf(out, "!"); break;
            }
            codegen_emit_expression(out, node->as.unary.operand);
            fprintf(out, ")");
            break;
        }

        case AST_FUNC_CALL:
            fprintf(out, "%.*s(", (int)node->as.func_call.name_length,
                    node->as.func_call.name_start);
            for (size_t i = 0; i < node->as.func_call.arg_count; i++) {
                if (i > 0) fprintf(out, ", ");
                codegen_emit_expression(out, node->as.func_call.arguments[i]);
            }
            fprintf(out, ")");
            break;

        case AST_VAL_DECL:
        case AST_MUT_DECL:
        case AST_RETURN:
        case AST_ASSIGNMENT:
        case AST_ASSERT:
        case AST_IF:
        case AST_WHILE:
        case AST_BREAK:
        case AST_CONTINUE:
        case AST_BLOCK:
        case AST_FUNC_DECL:
        case AST_PROGRAM:
            /* These should never appear in expressions */
            panic(ERR_I002_INTERNAL_ERROR, "statement node in expression context");
            break;
    }
}

static void codegen_indent(FILE *out, int indent) {
    for (int i = 0; i < indent; i++) {
        fprintf(out, "    ");
    }
}

static const char *codegen_type_to_c(Type type) {
    switch (type) {
        case TYPE_UNKNOWN:       return "long";   /* should not happen */
        case TYPE_I64:           return "long";
        case TYPE_F64:           return "double";
        case TYPE_BOOL:          return "int";
        case TYPE_VOID:          return "void";
        case TYPE_COMPTIME_INT:  return "long";   /* default */
        case TYPE_COMPTIME_FLOAT: return "double"; /* default */
    }
    return "long";
}

static void codegen_emit_statement(FILE *out, Ast *node, Scope **scope, int indent) {
    switch (node->kind) {
        case AST_VAL_DECL:
            scope_add(*scope, node->as.val_decl.name_start,
                      node->as.val_decl.name_length, false,
                      node->as.val_decl.type, node->loc);
            codegen_indent(out, indent);
            fprintf(out, "const %s %.*s = ",
                    codegen_type_to_c(node->as.val_decl.type),
                    (int)node->as.val_decl.name_length,
                    node->as.val_decl.name_start);
            codegen_emit_expression(out, node->as.val_decl.initializer);
            fprintf(out, ";\n");
            break;

        case AST_MUT_DECL:
            scope_add(*scope, node->as.mut_decl.name_start,
                      node->as.mut_decl.name_length, true,
                      node->as.mut_decl.type, node->loc);
            codegen_indent(out, indent);
            fprintf(out, "%s %.*s = ",
                    codegen_type_to_c(node->as.mut_decl.type),
                    (int)node->as.mut_decl.name_length,
                    node->as.mut_decl.name_start);
            codegen_emit_expression(out, node->as.mut_decl.initializer);
            fprintf(out, ";\n");
            break;

        case AST_RETURN:
            codegen_indent(out, indent);
            if (node->as.return_stmt.value) {
                fprintf(out, "return ");
                codegen_emit_expression(out, node->as.return_stmt.value);
                fprintf(out, ";\n");
            } else {
                fprintf(out, "return;\n");
            }
            break;

        case AST_ASSIGNMENT: {
            const char *name_start = node->as.assignment.name_start;
            size_t name_length = node->as.assignment.name_length;

            Variable *v = scope_lookup(*scope, name_start, name_length);
            if (v == NULL) {
                diagnostic(ERR_S002_UNDECLARED_VARIABLE, node->loc.line,
                           node->loc.column, "Undeclared variable '%.*s'",
                           (int)name_length, name_start);
                break;  /* Skip code generation */
            }
            if (!v->is_mutable) {
                diagnostic(ERR_S003_IMMUTABLE_ASSIGNMENT, node->loc.line,
                           node->loc.column,
                           "Cannot assign to immutable variable '%.*s'",
                           (int)name_length, name_start);
                break;  /* Skip code generation */
            }

            codegen_indent(out, indent);
            fprintf(out, "%.*s = ", (int)name_length, name_start);
            codegen_emit_expression(out, node->as.assignment.value);
            fprintf(out, ";\n");
            break;
        }

        case AST_ASSERT:
            codegen_indent(out, indent);
            fprintf(out, "if (!(");
            codegen_emit_expression(out, node->as.assert_stmt.condition);
            fprintf(out, ")) {\n");
            codegen_indent(out, indent + 1);
            fprintf(out, "fprintf(stderr, \"%s:%zu:%zu: error[R001]: Assertion failed\\n\");\n",
                    g_source_file, node->loc.line, node->loc.column);
            codegen_indent(out, indent + 1);
            fprintf(out, "exit(%d);\n", EXIT_RUNTIME);
            codegen_indent(out, indent);
            fprintf(out, "}\n");
            break;

        case AST_IF: {
            Ast *then_block = node->as.if_stmt.then_block;
            Ast *else_block = node->as.if_stmt.else_block;

            codegen_indent(out, indent);
            fprintf(out, "if (");
            codegen_emit_expression(out, node->as.if_stmt.condition);
            fprintf(out, ") {\n");

            /* Enter scope for then block */
            *scope = scope_create(*scope);
            for (size_t i = 0; i < then_block->as.block.count; i++) {
                codegen_emit_statement(out, then_block->as.block.statements[i], scope, indent + 1);
            }
            Scope *old = *scope;
            *scope = old->parent;
            scope_destroy(old);

            codegen_indent(out, indent);
            fprintf(out, "}");

            if (else_block) {
                fprintf(out, " else {\n");

                /* Enter scope for else block */
                *scope = scope_create(*scope);
                for (size_t i = 0; i < else_block->as.block.count; i++) {
                    codegen_emit_statement(out, else_block->as.block.statements[i], scope, indent + 1);
                }
                old = *scope;
                *scope = old->parent;
                scope_destroy(old);

                codegen_indent(out, indent);
                fprintf(out, "}");
            }
            fprintf(out, "\n");
            break;
        }

        case AST_WHILE: {
            Ast *body = node->as.while_stmt.body;

            codegen_indent(out, indent);
            fprintf(out, "while (");
            codegen_emit_expression(out, node->as.while_stmt.condition);
            fprintf(out, ") {\n");

            /* Enter scope for while body with incremented loop depth */
            *scope = scope_create(*scope);
            (*scope)->loop_depth++;
            for (size_t i = 0; i < body->as.block.count; i++) {
                codegen_emit_statement(out, body->as.block.statements[i], scope, indent + 1);
            }
            Scope *old = *scope;
            *scope = old->parent;
            scope_destroy(old);

            codegen_indent(out, indent);
            fprintf(out, "}\n");
            break;
        }

        case AST_BREAK:
            if ((*scope)->loop_depth == 0) {
                diagnostic(ERR_S004_BREAK_OUTSIDE_LOOP, node->loc.line,
                           node->loc.column, "'break' outside of loop");
                break;  /* Skip code generation */
            }
            codegen_indent(out, indent);
            fprintf(out, "break;\n");
            break;

        case AST_CONTINUE:
            if ((*scope)->loop_depth == 0) {
                diagnostic(ERR_S005_CONTINUE_OUTSIDE_LOOP, node->loc.line,
                           node->loc.column, "'continue' outside of loop");
                break;  /* Skip code generation */
            }
            codegen_indent(out, indent);
            fprintf(out, "continue;\n");
            break;

        case AST_BLOCK: {
            /* Enter new scope */
            *scope = scope_create(*scope);

            codegen_indent(out, indent);
            fprintf(out, "{\n");
            for (size_t i = 0; i < node->as.block.count; i++) {
                codegen_emit_statement(out, node->as.block.statements[i], scope, indent + 1);
            }
            codegen_indent(out, indent);
            fprintf(out, "}\n");

            /* Exit scope */
            Scope *old = *scope;
            *scope = old->parent;
            scope_destroy(old);
            break;
        }

        default:
            panic(ERR_I002_INTERNAL_ERROR, "invalid statement type in code generation");
    }
}

static void codegen_emit_function(FILE *out, Ast *func_decl) {
    const char *name_start = func_decl->as.func_decl.name_start;
    size_t name_length = func_decl->as.func_decl.name_length;
    Type return_type = func_decl->as.func_decl.return_type;

    /* Special case: main function generates int main(void) */
    bool is_main = (name_length == 4 && memcmp(name_start, "main", 4) == 0);

    if (is_main) {
        fprintf(out, "int main(");
    } else {
        fprintf(out, "%s %.*s(", codegen_type_to_c(return_type),
                (int)name_length, name_start);
    }

    /* Emit parameters */
    if (func_decl->as.func_decl.param_count == 0) {
        fprintf(out, "void");
    } else {
        for (size_t i = 0; i < func_decl->as.func_decl.param_count; i++) {
            Parameter *param = &func_decl->as.func_decl.params[i];
            if (i > 0) fprintf(out, ", ");
            fprintf(out, "%s %.*s", codegen_type_to_c(param->type),
                    (int)param->name_length, param->name_start);
        }
    }

    fprintf(out, ") {\n");

    /* Create scope with parameters */
    Scope *scope = scope_create(NULL);
    for (size_t i = 0; i < func_decl->as.func_decl.param_count; i++) {
        Parameter *param = &func_decl->as.func_decl.params[i];
        scope_add(scope, param->name_start, param->name_length,
                  false, param->type, func_decl->loc);
    }

    /* Emit body */
    Ast *body = func_decl->as.func_decl.body;
    for (size_t i = 0; i < body->as.block.count; i++) {
        codegen_emit_statement(out, body->as.block.statements[i], &scope, 1);
    }

    scope_destroy(scope);

    fprintf(out, "}\n\n");
}

static void codegen_emit_ir(FILE *out, Ast *ast) {
    fprintf(out, "#include <stdio.h>\n");
    fprintf(out, "#include <stdlib.h>\n\n");

    if (ast->kind != AST_PROGRAM) {
        panic(ERR_I002_INTERNAL_ERROR, "codegen_emit_ir expects AST_PROGRAM");
    }

    /* Emit all functions */
    for (size_t i = 0; i < ast->as.program.count; i++) {
        Ast *node = ast->as.program.statements[i];
        if (node->kind == AST_FUNC_DECL) {
            codegen_emit_function(out, node);
        }
    }
}

static void codegen_print_ir(Ast *ast) {
    printf("=== IR (C CODE) ===\n");
    codegen_emit_ir(stdout, ast);
    printf("\n");
}

static void codegen_compile_with_clang(const char *c_source_path, const char *binary_path) {
    char command[1024];
    int written = snprintf(command, sizeof(command),
                          "clang -std=c99 -O2 -o '%s' '%s' 2>&1",
                          binary_path, c_source_path);

    if (written < 0 || written >= (int)sizeof(command)) {
        panic(ERR_I002_INTERNAL_ERROR, "command buffer too small");
    }

    int status = system(command);

    if (status != 0) {
        error(ERR_D006_CLANG_FAILED,
                     "Clang compilation failed with exit code %d",
                     WEXITSTATUS(status));
    }
}

static void codegen_compile(Ast *ast, const char *output_path) {
    /* Create temporary C file */
    char temp_path[] = "/tmp/nore_XXXXXX.c";
    int fd = mkstemps(temp_path, 2);
    if (fd == -1) {
        panic(ERR_I002_INTERNAL_ERROR,
              "failed to create temporary file: %s", strerror(errno));
    }

    FILE *out = fdopen(fd, "w");
    if (!out) {
        close(fd);
        unlink(temp_path);
        panic(ERR_I002_INTERNAL_ERROR,
              "failed to open temporary file: %s", strerror(errno));
    }

    /* Generate C program */
    codegen_emit_ir(out, ast);

    fclose(out);

    /* Don't compile if there were errors */
    if (g_had_error) {
        unlink(temp_path);
        return;
    }

    /* Compile with Clang */
    codegen_compile_with_clang(temp_path, output_path);

    /* Cleanup temporary file */
    unlink(temp_path);
}

/* ================================ Compiler Flags ========================== */

typedef struct {
    int print_tokens;
    int print_ast;
    int print_ir;
} CompilerFlags;

/* =================================== Main ================================= */

int main(int argc, char **argv) {
    if (argc < 2) {
        error(ERR_D001_NO_INPUT_FILE,
                     "Usage: %s <file.nore> [--lexer] [--parser] [--codegen] [-o output]",
                     argv[0]);
    }

    const char *input_path = NULL;
    const char *output_arg = NULL;
    CompilerFlags flags = {0};

    /* Parse command-line arguments */
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--lexer") == 0) {
            flags.print_tokens = 1;
        } else if (strcmp(argv[i], "--parser") == 0) {
            flags.print_ast = 1;
        } else if (strcmp(argv[i], "--codegen") == 0) {
            flags.print_ir = 1;
        } else if (strcmp(argv[i], "-o") == 0) {
            if (i + 1 >= argc) {
                error(ERR_D004_MISSING_OUTPUT_PATH,
                             "Expected output path after -o");
            }
            output_arg = argv[++i];
        } else if (argv[i][0] == '-') {
            error(ERR_D002_UNKNOWN_FLAG, "Unknown flag: %s", argv[i]);
        } else {
            if (input_path) {
                error(ERR_D003_MULTIPLE_INPUTS,
                             "Multiple input files specified");
            }
            input_path = argv[i];
        }
    }

    if (!input_path) {
        error(ERR_D001_NO_INPUT_FILE, "No input file specified");
    }

    /* Set global source file for diagnostics */
    g_source_file = input_path;

    /* Determine output path */
    char output_path[256];
    if (output_arg) {
        strncpy(output_path, output_arg, sizeof(output_path) - 1);
        output_path[sizeof(output_path) - 1] = '\0';
    } else {
        /* Strip .nore extension or use a.out */
        const char *base = strrchr(input_path, '/');
        base = base ? base + 1 : input_path;
        const char *dot = strrchr(base, '.');
        if (dot && strcmp(dot, ".nore") == 0) {
            snprintf(output_path, sizeof(output_path), "%.*s",
                    (int)(dot - base), base);
        } else {
            snprintf(output_path, sizeof(output_path), "a.out");
        }
    }

    /* Read source file */
    size_t length;
    char *source = read_file(input_path, &length);

    Lexer lexer;
    lexer_init(&lexer, source);

    /* --lexer: Print tokens */
    if (flags.print_tokens) {
        lexer_print_tokens(&lexer, source);
    }

    /* Parse program */
    Parser parser;
    parser_init(&parser, &lexer);
    Ast *ast = parser_parse_program(&parser);

    /* --parser: Print AST */
    if (flags.print_ast) {
        parser_print_ast(ast);
    }

    /* Type checking */
    typecheck_program(ast);

    /* --codegen: Print intermediate C code (also runs semantic checks) */
    if (flags.print_ir) {
        codegen_print_ir(ast);
    }

    /* Skip compilation if any debug flag is set */
    int skip_compilation = flags.print_tokens || flags.print_ast || flags.print_ir;

    if (!skip_compilation) {
        /* Generate and compile */
        codegen_compile(ast, output_path);
    }

    /* Report any collected errors */
    if (g_had_error) {
        ast_free(ast);
        free(source);
        report_errors_and_exit();
    }

    /* Cleanup */
    ast_free(ast);
    free(source);

    return 0;
}

