#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <sys/wait.h>

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

/* ================================== AST =================================== */

typedef enum {
    AST_NUMBER,
    AST_IDENTIFIER,
    AST_BINARY,
    AST_UNARY,
    AST_VAL_DECL,
    AST_MUT_DECL,
    AST_RETURN,
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
            struct Ast **statements;
            size_t count;
            size_t capacity;
        } program;
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

static Ast *ast_make_unary(UnaryOp op, Ast *operand) {
    Ast *node = malloc(sizeof(Ast));
    if (!node) {
        error("Out of memory allocating AST node");
    }
    node->kind = AST_UNARY;
    node->as.unary.op = op;
    node->as.unary.operand = operand;
    return node;
}

static Ast *ast_make_val_decl(const char *name_start, size_t name_length, Ast *initializer) {
    Ast *node = malloc(sizeof(Ast));
    if (!node) {
        error("Out of memory allocating AST node");
    }
    node->kind = AST_VAL_DECL;
    node->as.val_decl.name_start = name_start;
    node->as.val_decl.name_length = name_length;
    node->as.val_decl.initializer = initializer;
    return node;
}

static Ast *ast_make_mut_decl(const char *name_start, size_t name_length, Ast *initializer) {
    Ast *node = malloc(sizeof(Ast));
    if (!node) {
        error("Out of memory allocating AST node");
    }
    node->kind = AST_MUT_DECL;
    node->as.mut_decl.name_start = name_start;
    node->as.mut_decl.name_length = name_length;
    node->as.mut_decl.initializer = initializer;
    return node;
}

static Ast *ast_make_return(Ast *value) {
    Ast *node = malloc(sizeof(Ast));
    if (!node) {
        error("Out of memory allocating AST node");
    }
    node->kind = AST_RETURN;
    node->as.return_stmt.value = value;
    return node;
}

static Ast *ast_make_program(void) {
    Ast *node = malloc(sizeof(Ast));
    if (!node) {
        error("Out of memory allocating AST node");
    }
    node->kind = AST_PROGRAM;
    node->as.program.statements = malloc(8 * sizeof(Ast*));
    if (!node->as.program.statements) {
        free(node);
        error("Out of memory allocating program statements");
    }
    node->as.program.count = 0;
    node->as.program.capacity = 8;
    return node;
}

static void ast_program_add_statement(Ast *program, Ast *statement) {
    if (program->kind != AST_PROGRAM) {
        error("Internal error: ast_program_add_statement called on non-program node");
    }

    if (program->as.program.count >= program->as.program.capacity) {
        size_t new_capacity = program->as.program.capacity * 2;
        Ast **new_statements = realloc(program->as.program.statements,
                                       new_capacity * sizeof(Ast*));
        if (!new_statements) {
            error("Out of memory growing program statements");
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
        Ast *operand = parser_parse_primary(parser);
        return ast_make_unary(OP_NEG, operand);
    }

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
        Ast *expr = parser_parse_expression(parser);
        parser_consume(parser, TOKEN_RPAREN, "Expected ')' after expression");
        return expr;
    }

    parser_error(parser, "Expected expression");
    return NULL;
}

static Ast *parser_parse_precedence(Parser *parser, int min_precedence) {
    /* Parse left operand */
    Ast *left = parser_parse_primary(parser);

    /* Consume operators while precedence is high enough */
    while (parser_get_precedence(parser->current.kind) >= min_precedence) {
        TokenKind op_kind = parser->current.kind;
        int precedence = parser_get_precedence(op_kind);
        parser_advance(parser);

        /* Parse right operand with higher precedence for left associativity */
        Ast *right = parser_parse_precedence(parser, precedence + 1);

        /* Combine into binary operation */
        left = ast_make_binary(parser_token_to_binary_op(op_kind), left, right);
    }

    return left;
}

static Ast *parser_parse_expression(Parser *parser) {
    return parser_parse_precedence(parser, 0);
}

/* ============================= Statement Parsing ========================== */

static Ast *parser_parse_val_declaration(Parser *parser) {
    /* Already consumed TOKEN_VAL */

    /* Expect identifier */
    if (!parser_match(parser, TOKEN_IDENTIFIER)) {
        parser_error(parser, "Expected identifier after 'val'");
    }
    const char *name_start = parser->previous.start;
    size_t name_length = parser->previous.length;

    /* Expect '=' */
    parser_consume(parser, TOKEN_EQUALS, "Expected '=' in val declaration");

    /* Parse initializer expression */
    Ast *initializer = parser_parse_expression(parser);

    return ast_make_val_decl(name_start, name_length, initializer);
}

static Ast *parser_parse_mut_declaration(Parser *parser) {
    /* Already consumed TOKEN_MUT */

    /* Expect identifier */
    if (!parser_match(parser, TOKEN_IDENTIFIER)) {
        parser_error(parser, "Expected identifier after 'mut'");
    }
    const char *name_start = parser->previous.start;
    size_t name_length = parser->previous.length;

    /* Expect '=' */
    parser_consume(parser, TOKEN_EQUALS, "Expected '=' in mut declaration");

    /* Parse initializer expression */
    Ast *initializer = parser_parse_expression(parser);

    return ast_make_mut_decl(name_start, name_length, initializer);
}

static Ast *parser_parse_return(Parser *parser) {
    /* Already consumed TOKEN_RETURN */

    /* Parse return value expression */
    Ast *value = parser_parse_expression(parser);

    return ast_make_return(value);
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

    if (parser_check(parser, TOKEN_EOF)) {
        return NULL;
    }

    parser_error(parser, "Expected statement (val, mut, or return)");
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
        case AST_PROGRAM:
            error("Code generation not yet implemented for statements");
            break;
    }
}

static void codegen_emit_ir(FILE *out, Ast *ast) {
    fprintf(out, "#include <stdio.h>\n");
    fprintf(out, "int main() {\n");
    fprintf(out, "    long result = ");
    codegen_emit_expression(out, ast);
    fprintf(out, ";\n");
    fprintf(out, "    printf(\"%%ld\\n\", result);\n");
    fprintf(out, "    return 0;\n");
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
        error("Command buffer too small");
    }

    int status = system(command);

    if (status != 0) {
        error("Clang compilation failed with exit code %d",
              WEXITSTATUS(status));
    }
}

static void codegen_compile(Ast *ast, const char *output_path) {
    /* Create temporary C file */
    char temp_path[] = "/tmp/nore_XXXXXX.c";
    int fd = mkstemps(temp_path, 2);
    if (fd == -1) {
        error("Failed to create temporary file: %s", strerror(errno));
    }

    FILE *out = fdopen(fd, "w");
    if (!out) {
        close(fd);
        unlink(temp_path);
        error("Failed to open temporary file: %s", strerror(errno));
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
        error("Usage: %s <file.nore> [--lexer] [--parser] [--codegen] [-o output]", argv[0]);
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
                error("Expected output path after -o");
            }
            output_arg = argv[++i];
        } else if (argv[i][0] == '-') {
            error("Unknown flag: %s", argv[i]);
        } else {
            if (input_path) {
                error("Multiple input files specified");
            }
            input_path = argv[i];
        }
    }

    if (!input_path) {
        error("No input file specified");
    }

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

