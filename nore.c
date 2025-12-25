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

/* ================================== AST =================================== */

typedef enum {
    AST_NUMBER,
    AST_IDENTIFIER,
    AST_BINARY
} AstKind;

typedef enum {
    OP_ADD,
    OP_SUB,
    OP_MUL,
    OP_DIV
} BinaryOp;

typedef struct Ast {
    AstKind kind;
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
    } as;
} Ast;

/* ============================== AST Allocators ============================ */

static Ast *ast_make_number(long value) {
    Ast *node = malloc(sizeof(Ast));
    if (!node) {
        error("Out of memory allocating AST node");
    }
    node->kind = AST_NUMBER;
    node->as.number.value = value;
    return node;
}

static Ast *ast_make_identifier(const char *start, size_t length) {
    Ast *node = malloc(sizeof(Ast));
    if (!node) {
        error("Out of memory allocating AST node");
    }
    node->kind = AST_IDENTIFIER;
    node->as.identifier.start = start;
    node->as.identifier.length = length;
    return node;
}

static Ast *ast_make_binary(BinaryOp op, Ast *left, Ast *right) {
    Ast *node = malloc(sizeof(Ast));
    if (!node) {
        error("Out of memory allocating AST node");
    }
    node->kind = AST_BINARY;
    node->as.binary.op = op;
    node->as.binary.left = left;
    node->as.binary.right = right;
    return node;
}

static void ast_free(Ast *node) {
    if (!node) return;
    if (node->kind == AST_BINARY) {
        ast_free(node->as.binary.left);
        ast_free(node->as.binary.right);
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

static void parser_error(Parser *parser, const char *message) {
    Token *token = &parser->current;
    fprintf(stderr, "Error at line %zu col %zu: %s\n",
            token->line, token->column, message);
    exit(1);
}

static void parser_consume(Parser *parser, TokenKind kind, const char *msg) {
    if (!parser_match(parser, kind)) {
        parser_error(parser, msg);
    }
}

/* ============================ Operator Precedence ========================= */

static int get_precedence(TokenKind kind) {
    switch (kind) {
        case TOKEN_STAR:
        case TOKEN_SLASH:
            return 2;
        case TOKEN_PLUS:
        case TOKEN_MINUS:
            return 1;
        default:
            return -1;
    }
}

static BinaryOp token_to_binary_op(TokenKind kind) {
    switch (kind) {
        case TOKEN_PLUS:  return OP_ADD;
        case TOKEN_MINUS: return OP_SUB;
        case TOKEN_STAR:  return OP_MUL;
        case TOKEN_SLASH: return OP_DIV;
        default:
            fprintf(stderr, "Internal error: not a binary operator\n");
            exit(1);
    }
}

/* ============================= Expression Parsing ========================= */

static Ast *parse_expression(Parser *parser);

static Ast *parse_primary(Parser *parser) {
    if (parser_match(parser, TOKEN_NUMBER)) {
        char buffer[32];
        size_t len = parser->previous.length;
        if (len >= sizeof(buffer)) {
            parser_error(parser, "Number too large");
        }
        memcpy(buffer, parser->previous.start, len);
        buffer[len] = '\0';
        long value = strtol(buffer, NULL, 10);
        return ast_make_number(value);
    }

    if (parser_match(parser, TOKEN_IDENTIFIER)) {
        return ast_make_identifier(parser->previous.start, parser->previous.length);
    }

    if (parser_match(parser, TOKEN_LPAREN)) {
        Ast *expr = parse_expression(parser);
        parser_consume(parser, TOKEN_RPAREN, "Expected ')' after expression");
        return expr;
    }

    parser_error(parser, "Expected expression");
    return NULL;
}

static Ast *parse_precedence(Parser *parser, int min_precedence) {
    /* Parse left operand */
    Ast *left = parse_primary(parser);

    /* Consume operators while precedence is high enough */
    while (get_precedence(parser->current.kind) >= min_precedence) {
        TokenKind op_kind = parser->current.kind;
        int precedence = get_precedence(op_kind);
        parser_advance(parser);

        /* Parse right operand with higher precedence for left associativity */
        Ast *right = parse_precedence(parser, precedence + 1);

        /* Combine into binary operation */
        left = ast_make_binary(token_to_binary_op(op_kind), left, right);
    }

    return left;
}

static Ast *parse_expression(Parser *parser) {
    return parse_precedence(parser, 0);
}

/* ============================== AST Printer =============================== */

static void ast_print(Ast *node, int indent) {
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
                default: op_name = "UNKNOWN"; break;
            }
            printf("BINARY(%s)\n", op_name);
            ast_print(node->as.binary.left, indent + 1);
            ast_print(node->as.binary.right, indent + 1);
            break;
        }
    }
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

    Parser parser;
    parser_init(&parser, &lexer);

    Ast *ast = parse_expression(&parser);

    ast_print(ast, 0);

    ast_free(ast);
    free(source);

    return 0;
}

