#ifndef EXPRESSION_H
#define EXPRESSION_H

#include "ast.h"
#include "lexer.h"
#include "parser.h"

static const int operator_precedences[] = {
  [OP_EQ] = 1, [OP_NEQ] = 1,
  [OP_GT] = 1, [OP_LT] = 1, [OP_GTE] = 1, [OP_LTE] = 1,
  [OP_BOR] = 2, [OP_BXOR] = 3, [OP_BAND] = 4,
  [OP_SHL] = 5, [OP_SHR] = 5,
  [OP_ADD] = 6, [OP_SUB] = 6,
  [OP_MUL] = 7, [OP_DIV] = 7, [OP_MOD] = 7,
  [OP_NEG] = 8,
};

static const bool operator_is_left_associatives[] = {
  [OP_EQ] = true, [OP_NEQ] = true,
  [OP_GT] = true, [OP_LT] = true, [OP_GTE] = true, [OP_LTE] = true,
  [OP_BOR] = true, [OP_BXOR] = true, [OP_BAND] = true,
  [OP_SHL] = true, [OP_SHR] = true,
  [OP_ADD] = true, [OP_SUB] = true,
  [OP_MUL] = true, [OP_DIV] = true, [OP_MOD] = true,
  [OP_NEG] = false,
};

static const bool operator_is_unarys[] = {
  [OP_EQ] = false, [OP_NEQ] = false,
  [OP_GT] = false, [OP_LT] = false, [OP_GTE] = false, [OP_LTE] = false,
  [OP_BOR] = false, [OP_BXOR] = false, [OP_BAND] = false,
  [OP_SHL] = false, [OP_SHR] = false,
  [OP_ADD] = false, [OP_SUB] = false,
  [OP_MUL] = false, [OP_DIV] = false, [OP_MOD] = false,
  [OP_NEG] = true,
};

int operator_precedence(expr_op op);
bool operator_is_left_associative(expr_op op);
bool operator_is_unary(expr_op op);

bool parser_parse_expression(struct parser *parser, struct ast_expr *expr);

#endif
