#ifndef PARSER_H
#define PARSER_H

#include <stdbool.h>

#include "lexer.h"
#include "ast.h"

struct parser {
  bool error;
  struct arena *arena;
  struct lexer *lex;
};

struct parser parser_new(struct arena *arena, struct lexer *lex);

bool parser_expect(struct parser *parser, token_type tt, struct token *tk);

bool parser_parse_ast(struct parser *parser, struct ast *ast);
bool parser_parse_const(struct parser *parser, struct ast_const *c);
bool parser_parse_expression(struct parser *parser, struct ast_expr *expr);
bool parser_parse_proc(struct parser *parser, struct ast_const *c);
bool parser_parse_stmt(struct parser *parser, struct ast_stmt *proc);
bool parser_parse_let(struct parser *parser, struct ast_assign *assign);
bool parser_parse_assign(struct parser *parser, struct ast_assign *assign);
bool parser_parse_if(struct parser *parser, struct ast_if *if_);

#endif /* PARSER_H */
