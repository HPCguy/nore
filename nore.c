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
#define EXIT_INTERNAL 101

/* Error code group offsets */
#define ERR_GROUP_INTERNAL 1000
#define ERR_GROUP_DRIVER   2000
#define ERR_GROUP_LEXER    3000
#define ERR_GROUP_PARSER   4000
#define ERR_GROUP_SEMANTIC 5000

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
    ERR_P001_EXPECTED_EXPRESSION = ERR_GROUP_PARSER + 1,
    ERR_P002_EXPECTED_RPAREN     = ERR_GROUP_PARSER + 2,
    ERR_P003_EXPECTED_IDENTIFIER = ERR_GROUP_PARSER + 3,
    ERR_P004_EXPECTED_EQUALS     = ERR_GROUP_PARSER + 4,
    ERR_P005_EXPECTED_STATEMENT  = ERR_GROUP_PARSER + 5,
    ERR_P006_NUMBER_TOO_LARGE    = ERR_GROUP_PARSER + 6,

    /* Semantic errors: S001-S099 */
    ERR_S001_DUPLICATE_VARIABLE    = ERR_GROUP_SEMANTIC + 1,
    ERR_S002_UNDECLARED_VARIABLE   = ERR_GROUP_SEMANTIC + 2,
    ERR_S003_IMMUTABLE_ASSIGNMENT  = ERR_GROUP_SEMANTIC + 3,
} ErrorCode;

static const char *error_code_str(ErrorCode code) {
    static char buffer[8];
    char prefix;
    int num;

    if (code >= ERR_GROUP_SEMANTIC) {
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

/* User errors with source location */
static void diagnostic(ErrorCode code, size_t line, size_t column,
                       const char *fmt, ...) {
    va_list args;
    va_start(args, fmt);
    fprintf(stderr, "%s:%zu:%zu: error[%s]: ", g_source_file, line, column,
            error_code_str(code));
    vfprintf(stderr, fmt, args);
    fprintf(stderr, "\n");
    va_end(args);
    exit(EXIT_USER_ERROR);
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
    TOKEN_EQUAL_EQUAL,
    TOKEN_BANG_EQUAL,
    TOKEN_LESS,
    TOKEN_GREATER,
    TOKEN_LESS_EQUAL,
    TOKEN_GREATER_EQUAL,

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
        case TOKEN_IDENTIFIER:    return "IDENTIFIER";
        case TOKEN_FUNC:          return "FUNC";
        case TOKEN_VAL:           return "VAL";
        case TOKEN_MUT:           return "MUT";
        case TOKEN_RETURN:        return "RETURN";
        case TOKEN_VOID:          return "VOID";
        case TOKEN_I32:           return "I32";
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
    return lexer_make_token(lexer, TOKEN_NUMBER);
}

static TokenKind lexer_identify_keyword(const char *start, size_t length) {
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
            return lexer_error_token(lexer, "Unexpected character '!'");
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
        if (token.kind == TOKEN_NUMBER || token.kind == TOKEN_IDENTIFIER) {
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

/* ================================== AST =================================== */

typedef enum {
    AST_NUMBER,
    AST_IDENTIFIER,
    AST_BINARY,
    AST_UNARY,
    AST_VAL_DECL,
    AST_MUT_DECL,
    AST_RETURN,
    AST_ASSIGNMENT,
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
    OP_GE
} BinaryOp;

typedef enum {
    OP_NEG
} UnaryOp;

typedef struct Ast {
    AstKind kind;
    SourceLoc loc;
    union {
        struct {
            long value;
        } number;

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
            struct Ast *initializer;
        } val_decl;

        struct {
            const char *name_start;
            size_t name_length;
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

static Ast *ast_make_val_decl(const char *name_start, size_t name_length,
                              Ast *initializer, SourceLoc loc) {
    Ast *node = malloc(sizeof(Ast));
    if (!node) {
        panic(ERR_I001_OUT_OF_MEMORY, "allocating AST node");
    }
    node->kind = AST_VAL_DECL;
    node->loc = loc;
    node->as.val_decl.name_start = name_start;
    node->as.val_decl.name_length = name_length;
    node->as.val_decl.initializer = initializer;
    return node;
}

static Ast *ast_make_mut_decl(const char *name_start, size_t name_length,
                              Ast *initializer, SourceLoc loc) {
    Ast *node = malloc(sizeof(Ast));
    if (!node) {
        panic(ERR_I001_OUT_OF_MEMORY, "allocating AST node");
    }
    node->kind = AST_MUT_DECL;
    node->loc = loc;
    node->as.mut_decl.name_start = name_start;
    node->as.mut_decl.name_length = name_length;
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

static void ast_free(Ast *node) {
    if (!node) return;
    if (node->kind == AST_BINARY) {
        ast_free(node->as.binary.left);
        ast_free(node->as.binary.right);
    }
    if (node->kind == AST_UNARY) {
        ast_free(node->as.unary.operand);
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

static void parser_consume(Parser *parser, TokenKind kind, ErrorCode code,
                           const char *msg) {
    if (!parser_match(parser, kind)) {
        diagnostic(code, parser->current.line, parser->current.column, "%s", msg);
    }
}

static TokenKind parser_peek_next(Parser *parser) {
    /* Save lexer state */
    Lexer saved = *parser->lexer;
    Token next = lexer_next_token(parser->lexer);
    /* Restore lexer state */
    *parser->lexer = saved;
    return next.kind;
}

/* ============================ Operator Precedence ========================= */

static int parser_get_precedence(TokenKind kind) {
    switch (kind) {
        case TOKEN_STAR:
        case TOKEN_SLASH:
            return 2;
        case TOKEN_PLUS:
        case TOKEN_MINUS:
            return 1;
        case TOKEN_EQUAL_EQUAL:
        case TOKEN_BANG_EQUAL:
        case TOKEN_LESS:
        case TOKEN_GREATER:
        case TOKEN_LESS_EQUAL:
        case TOKEN_GREATER_EQUAL:
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
        default:
            fprintf(stderr, "Internal error: not a binary operator\n");
            exit(1);
    }
}

/* ============================= Expression Parsing ========================= */

static Ast *parser_parse_expression(Parser *parser);

static Ast *parser_parse_primary(Parser *parser) {
    /* Handle unary minus */
    if (parser_match(parser, TOKEN_MINUS)) {
        SourceLoc loc = token_loc(&parser->previous);
        Ast *operand = parser_parse_primary(parser);
        return ast_make_unary(OP_NEG, operand, loc);
    }

    if (parser_match(parser, TOKEN_NUMBER)) {
        SourceLoc loc = token_loc(&parser->previous);
        char buffer[32];
        size_t len = parser->previous.length;
        if (len >= sizeof(buffer)) {
            diagnostic(ERR_P006_NUMBER_TOO_LARGE, loc.line, loc.column,
                       "Number literal too large");
        }
        memcpy(buffer, parser->previous.start, len);
        buffer[len] = '\0';
        long value = strtol(buffer, NULL, 10);
        return ast_make_number(value, loc);
    }

    if (parser_match(parser, TOKEN_IDENTIFIER)) {
        SourceLoc loc = token_loc(&parser->previous);
        return ast_make_identifier(parser->previous.start, parser->previous.length, loc);
    }

    if (parser_match(parser, TOKEN_LPAREN)) {
        Ast *expr = parser_parse_expression(parser);
        parser_consume(parser, TOKEN_RPAREN, ERR_P002_EXPECTED_RPAREN,
                       "Expected ')' after expression");
        return expr;
    }

    diagnostic(ERR_P001_EXPECTED_EXPRESSION, parser->current.line, parser->current.column,
               "Expected expression");
    return NULL;
}

static Ast *parser_parse_precedence(Parser *parser, int min_precedence) {
    /* Parse left operand */
    Ast *left = parser_parse_primary(parser);

    /* Consume operators while precedence is high enough */
    while (parser_get_precedence(parser->current.kind) >= min_precedence) {
        TokenKind op_kind = parser->current.kind;
        SourceLoc op_loc = token_loc(&parser->current);
        int precedence = parser_get_precedence(op_kind);
        parser_advance(parser);

        /* Parse right operand with higher precedence for left associativity */
        Ast *right = parser_parse_precedence(parser, precedence + 1);

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
    }
    const char *name_start = parser->previous.start;
    size_t name_length = parser->previous.length;

    /* Expect '=' */
    if (!parser_match(parser, TOKEN_EQUALS)) {
        diagnostic(ERR_P004_EXPECTED_EQUALS, parser->current.line,
                   parser->current.column, "Expected '=' in val declaration");
    }

    /* Parse initializer expression */
    Ast *initializer = parser_parse_expression(parser);

    return ast_make_val_decl(name_start, name_length, initializer, loc);
}

static Ast *parser_parse_mut_declaration(Parser *parser) {
    /* Already consumed TOKEN_MUT - capture its location */
    SourceLoc loc = token_loc(&parser->previous);

    /* Expect identifier */
    if (!parser_match(parser, TOKEN_IDENTIFIER)) {
        diagnostic(ERR_P003_EXPECTED_IDENTIFIER, parser->current.line,
                   parser->current.column, "Expected identifier after 'mut'");
    }
    const char *name_start = parser->previous.start;
    size_t name_length = parser->previous.length;

    /* Expect '=' */
    if (!parser_match(parser, TOKEN_EQUALS)) {
        diagnostic(ERR_P004_EXPECTED_EQUALS, parser->current.line,
                   parser->current.column, "Expected '=' in mut declaration");
    }

    /* Parse initializer expression */
    Ast *initializer = parser_parse_expression(parser);

    return ast_make_mut_decl(name_start, name_length, initializer, loc);
}

static Ast *parser_parse_return(Parser *parser) {
    /* Already consumed TOKEN_RETURN - capture its location */
    SourceLoc loc = token_loc(&parser->previous);

    /* Parse return value expression */
    Ast *value = parser_parse_expression(parser);

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

    return ast_make_assignment(name_start, name_length, value, loc);
}

static Ast *parser_parse_statement(Parser *parser) {
    if (parser_match(parser, TOKEN_VAL)) {
        return parser_parse_val_declaration(parser);
    }

    if (parser_match(parser, TOKEN_MUT)) {
        return parser_parse_mut_declaration(parser);
    }

    if (parser_match(parser, TOKEN_RETURN)) {
        return parser_parse_return(parser);
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
    return NULL;
}

static Ast *parser_parse_program(Parser *parser) {
    Ast *program = ast_make_program();

    while (!parser_check(parser, TOKEN_EOF)) {
        Ast *statement = parser_parse_statement(parser);
        if (statement) {
            ast_program_add_statement(program, statement);
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
                default: op_name = "UNKNOWN"; break;
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
                default: op_name = "UNKNOWN"; break;
            }
            printf("UNARY(%s)\n", op_name);
            parser_print_ast_step(node->as.unary.operand, indent + 1);
            break;
        }

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
            parser_print_ast_step(node->as.return_stmt.value, indent + 1);
            break;

        case AST_ASSIGNMENT:
            printf("ASSIGNMENT(%.*s)\n",
                   (int)node->as.assignment.name_length,
                   node->as.assignment.name_start);
            parser_print_ast_step(node->as.assignment.value, indent + 1);
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

/* ============================== Variables ================================= */

typedef struct {
    const char *name_start;
    size_t name_length;
    bool is_mutable;
} Variable;

typedef struct {
    Variable *vars;
    size_t count;
    size_t capacity;
} VarTable;

static void vartable_init(VarTable *table) {
    table->vars = malloc(8 * sizeof(Variable));
    if (!table->vars) {
        panic(ERR_I001_OUT_OF_MEMORY, "allocating variable table");
    }
    table->count = 0;
    table->capacity = 8;
}

static void vartable_free(VarTable *table) {
    free(table->vars);
}

static Variable *vartable_lookup(VarTable *table, const char *name_start,
                                  size_t name_length) {
    for (size_t i = 0; i < table->count; i++) {
        if (table->vars[i].name_length == name_length &&
            memcmp(table->vars[i].name_start, name_start, name_length) == 0) {
            return &table->vars[i];
        }
    }
    return NULL;
}

static void vartable_add(VarTable *table, const char *name_start,
                         size_t name_length, bool is_mutable, SourceLoc loc) {
    /* Check for duplicates */
    if (vartable_lookup(table, name_start, name_length) != NULL) {
        diagnostic(ERR_S001_DUPLICATE_VARIABLE, loc.line, loc.column,
                   "Variable '%.*s' already declared", (int)name_length, name_start);
    }

    /* Grow if needed */
    if (table->count >= table->capacity) {
        table->capacity *= 2;
        table->vars = realloc(table->vars, table->capacity * sizeof(Variable));
        if (!table->vars) {
            panic(ERR_I001_OUT_OF_MEMORY, "growing variable table");
        }
    }

    /* Add variable */
    table->vars[table->count].name_start = name_start;
    table->vars[table->count].name_length = name_length;
    table->vars[table->count].is_mutable = is_mutable;
    table->count++;
}

/* ============================ Code Generation ============================= */

static void codegen_emit_expression(FILE *out, Ast *node);

static void codegen_emit_expression(FILE *out, Ast *node) {
    switch (node->kind) {
        case AST_NUMBER:
            fprintf(out, "%ldL", node->as.number.value);
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
            }

            codegen_emit_expression(out, node->as.binary.right);
            fprintf(out, ")");
            break;
        }

        case AST_UNARY: {
            fprintf(out, "(");
            switch (node->as.unary.op) {
                case OP_NEG: fprintf(out, "-"); break;
            }
            codegen_emit_expression(out, node->as.unary.operand);
            fprintf(out, ")");
            break;
        }

        case AST_VAL_DECL:
        case AST_MUT_DECL:
        case AST_RETURN:
        case AST_ASSIGNMENT:
        case AST_PROGRAM:
            /* These should never appear in expressions */
            panic(ERR_I002_INTERNAL_ERROR, "statement node in expression context");
            break;
    }
}

static void codegen_emit_statement(FILE *out, Ast *node, VarTable *table) {
    switch (node->kind) {
        case AST_VAL_DECL:
            vartable_add(table, node->as.val_decl.name_start,
                         node->as.val_decl.name_length, false, node->loc);
            fprintf(out, "    const long %.*s = ",
                    (int)node->as.val_decl.name_length,
                    node->as.val_decl.name_start);
            codegen_emit_expression(out, node->as.val_decl.initializer);
            fprintf(out, ";\n");
            break;

        case AST_MUT_DECL:
            vartable_add(table, node->as.mut_decl.name_start,
                         node->as.mut_decl.name_length, true, node->loc);
            fprintf(out, "    long %.*s = ",
                    (int)node->as.mut_decl.name_length,
                    node->as.mut_decl.name_start);
            codegen_emit_expression(out, node->as.mut_decl.initializer);
            fprintf(out, ";\n");
            break;

        case AST_RETURN:
            fprintf(out, "    return ");
            codegen_emit_expression(out, node->as.return_stmt.value);
            fprintf(out, ";\n");
            break;

        case AST_ASSIGNMENT: {
            const char *name_start = node->as.assignment.name_start;
            size_t name_length = node->as.assignment.name_length;

            Variable *v = vartable_lookup(table, name_start, name_length);
            if (v == NULL) {
                diagnostic(ERR_S002_UNDECLARED_VARIABLE, node->loc.line,
                           node->loc.column, "Undeclared variable '%.*s'",
                           (int)name_length, name_start);
            }
            if (!v->is_mutable) {
                diagnostic(ERR_S003_IMMUTABLE_ASSIGNMENT, node->loc.line,
                           node->loc.column,
                           "Cannot assign to immutable variable '%.*s'",
                           (int)name_length, name_start);
            }

            fprintf(out, "    %.*s = ", (int)name_length, name_start);
            codegen_emit_expression(out, node->as.assignment.value);
            fprintf(out, ";\n");
            break;
        }

        default:
            panic(ERR_I002_INTERNAL_ERROR, "invalid statement type in code generation");
    }
}

static void codegen_emit_ir(FILE *out, Ast *ast) {
    fprintf(out, "#include <stdio.h>\n");
    fprintf(out, "int main() {\n");

    if (ast->kind != AST_PROGRAM) {
        panic(ERR_I002_INTERNAL_ERROR, "codegen_emit_ir expects AST_PROGRAM");
    }

    VarTable table;
    vartable_init(&table);

    for (size_t i = 0; i < ast->as.program.count; i++) {
        codegen_emit_statement(out, ast->as.program.statements[i], &table);
    }

    vartable_free(&table);

    fprintf(out, "}\n");
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

    /* --codegen: Print intermediate C code */
    if (flags.print_ir) {
        codegen_print_ir(ast);
    }

    /* Skip compilation if any debug flag is set */
    int skip_compilation = flags.print_tokens || flags.print_ast || flags.print_ir;

    if (!skip_compilation) {
        /* Generate and compile */
        codegen_compile(ast, output_path);
    }

    /* Cleanup */
    ast_free(ast);
    free(source);

    return 0;
}

