#ifndef AST_H
#define AST_H

#include <stddef.h>
#include <stdint.h>

#include "lexer.h"

typedef enum {
  TERM_INT,
  TERM_IDENT,
  TERM_FN_CALL,
  EXPR_OPERATION,
} ast_expr_type;

typedef enum {
  OP_EQ = TT_EQ, OP_NEQ = TT_NEQ,
  OP_GT = TT_GT, OP_LT = TT_LT, OP_GTE = TT_GTE, OP_LTE = TT_LTE,
  OP_BOR = TT_BOR, OP_BXOR = TT_BXOR, OP_BAND = TT_BAND,
  OP_SHL = TT_SHL, OP_SHR = TT_SHR,
  OP_ADD = TT_ADD, OP_SUB = TT_SUB,
  OP_MUL = TT_MUL, OP_DIV = TT_DIV, OP_MOD = TT_MOD,
  OP_NEG,
} expr_op;

typedef enum {
  STMT_EXPR,
  STMT_RET,
  STMT_LET,
  STMT_ASSIGN,
  STMT_IF,
} ast_stmt_type;

typedef enum {
  CONST_PROC,
  CONST_EXPR,
  CONST_STRING,
  CONST_PROC_DECLARATION,
} ast_const_type;

struct ident {
  size_t len;
  const char *chars;
};

struct string {
  size_t len;
  const char *chars;
};

struct ast_expr;

struct ast_assign {
  struct ident ident;
  struct ast_expr *expr;
};

struct ast_if_branch {
  struct ast_expr *cond;
  size_t stmts_len;
  struct ast_stmt **stmts;
};

struct ast_if {
  size_t branches_len;
  struct ast_if_branch *branches;
  
  size_t else_stmts_len;
  struct ast_stmt **else_stmts;
};

union ast_stmt_as {
  struct ast_expr *expr, *ret;
  struct ast_assign *assign, *let;
  struct ast_if *if_;
};

struct ast_stmt {
  ast_stmt_type type;
  union ast_stmt_as as;
};

struct ast_operation {
  expr_op op;
  struct ast_expr *lhs, *rhs;
};

struct ast_proc {
  size_t args_len;
  struct ident *idents;
  size_t stmts_len;
  struct ast_stmt **stmts;
};


struct ast_fn_call {
  size_t args_len;
  struct ast_expr *fn, **args;
};

union ast_expr_as {
  int64_t integer;
  struct ident ident;
  struct ast_fn_call *fn_call;
  struct ast_operation op;
};

struct ast_expr {
  ast_expr_type type;
  union ast_expr_as as;
  size_t len;
  const char *loc;
};

struct ast_const_as {
  struct ast_proc proc;
  struct ast_expr *expr;
  struct string string;
};

struct ast_const {
  struct ident ident;
  ast_const_type type;
  struct ast_const_as as;
};

struct ast {
  size_t consts_len;
  struct ast_const *consts;
};

#endif /* AST_H */
