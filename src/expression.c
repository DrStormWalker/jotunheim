#include "expression.h"

#include <stdlib.h>
#include <string.h>

#include "error.h"
#include "lexer.h"
#include "ast.h"
#include "arena.h"

typedef enum {
  EPS_UNARY,
  EPS_BINARY,
  EPS_STOP,
} expression_parser_state;

typedef enum {
  MKR_BOR = OP_BOR, MKR_BXOR = OP_BXOR, MKR_BAND = OP_BAND,
  MKR_SHL = OP_SHL, MKR_SHR = OP_SHR,
  MKR_ADD = OP_ADD, MKR_SUB = OP_SUB,
  MKR_MUL = OP_MUL, MKR_DIV = OP_DIV, MKR_MOD = OP_MOD,
  MKR_NEG = OP_NEG,
  MKR_LBRACKET,
  MKR_FUNCTION,
} op_marker;

struct operation_item {
  op_marker op;
  size_t len;
  const char *loc;
};

struct expression_ctx {
  expression_parser_state state;
  struct arena *arena;
  struct parser *parser;

  size_t operand_stack_ptr, operator_stack_ptr;
  struct ast_expr *operand_stack[100];
  struct operation_item operator_stack[100];
};

int operator_precedence(expr_op op) {
  return operator_precedences[op];
}

bool operator_is_left_associative(expr_op op) {
  return operator_is_left_associatives[op];
}

bool operator_is_unary(expr_op op) {
  return operator_is_unarys[op];
}

static inline bool parser_error(struct parser *parser) {
  parser->error = true;
  return false;
}

bool expression_parser_should_pop(struct expression_ctx *ctx, expr_op op) {
  if (ctx->operator_stack_ptr <= 0)
    return false;

  expr_op stack_top = (expr_op)ctx->operator_stack[ctx->operator_stack_ptr - 1].op;
  int precedence_diff = operator_precedence(stack_top) - operator_precedence(op);

  // TODO: check for (

  return precedence_diff > 0 || precedence_diff == 0 && operator_is_left_associative(op);
}

void expression_parser_pop_op(struct expression_ctx *ctx) {
  struct operation_item op = ctx->operator_stack[--ctx->operator_stack_ptr];
  struct ast_expr *lhs, *rhs, *expr = arena_alloc(ctx->arena, sizeof(struct ast_expr));

  expr->type = EXPR_OPERATION;

  expr->as.op.op = (expr_op)op.op;

  if (!operator_is_unary((expr_op)op.op)) {
    rhs = ctx->operand_stack[--ctx->operand_stack_ptr];
    lhs = ctx->operand_stack[--ctx->operand_stack_ptr];

    expr->loc = lhs->loc;
    expr->len = rhs->loc + rhs->len - lhs->loc;
    expr->as.op.lhs = lhs;
    expr->as.op.rhs = rhs;
  } else {
    lhs = ctx->operand_stack[--ctx->operand_stack_ptr];

    if (lhs->loc < op.loc) {
      expr->loc = lhs->loc;
      expr->len = op.loc + op.len - lhs->loc;
    } else {
      expr->loc = op.loc;
      expr->len = lhs->loc + lhs->len - op.loc;
    }

    expr->as.op.lhs = lhs;
  }

  ctx->operand_stack[ctx->operand_stack_ptr++] = expr;
}

void expression_parser_push_op(struct expression_ctx *ctx, expr_op op, size_t len, const char *loc) {
  while (expression_parser_should_pop(ctx, op))
    expression_parser_pop_op(ctx);

  ctx->operator_stack[ctx->operator_stack_ptr++] = (struct operation_item) {
    .op = (op_marker)op,
    .len = len,
    .loc = loc,
  };
}

bool expression_parser_unary(struct expression_ctx *ctx, struct token *tk) {
  switch (tk->type) {
    case TT_INTEGER: {
      char *end;
      struct ast_expr *expr = arena_alloc(ctx->arena, sizeof(struct ast_expr));

      expr->as.integer = strtoll(tk->loc, &end, 10);
      expr->type = TERM_INT;
      expr->loc = tk->loc;
      expr->len = tk->len;

      ctx->operand_stack[ctx->operand_stack_ptr++] = expr;

      ctx->state = EPS_BINARY;
    } break;

    case TT_IDENT: {
      struct ast_expr *expr = arena_alloc(ctx->arena, sizeof(struct ast_expr));

      expr->as.ident.len = tk->len;
      expr->as.ident.chars = tk->loc;
      expr->type = TERM_IDENT;
      expr->loc = tk->loc;
      expr->len = tk->len;

      ctx->operand_stack[ctx->operand_stack_ptr++] = expr;

      ctx->state = EPS_BINARY;
    } break;

    case TT_STRING: {
      fprint_error(stderr, "use of string in expression");
      fprint_error_ctx(stderr, ctx->parser->lex->src, 1, 0, tk->len, tk->loc, "here");
      fprint_note(stderr, "currently Jotunheim only supports string definitions in constants");

      return parser_error(ctx->parser);
    } break;

    case TT_SUB: {
      expression_parser_push_op(ctx, OP_NEG, tk->len, tk->loc);
        
      ctx->state = EPS_UNARY;
    } break;

    case TT_L_BRACKET: {
      ctx->operator_stack[ctx->operator_stack_ptr++] = (struct operation_item) {
        .op = MKR_LBRACKET,
        .len = tk->len,
        .loc = tk->loc,
      };

      ctx->state = EPS_UNARY;
    } break;

    default: {
      ctx->state = EPS_STOP;
    } break;
  }

  return true;
}

bool expression_parser_binary(struct expression_ctx *ctx, struct token *tk) {
  switch (tk->type) {
    case TT_EQ: case TT_NEQ:
    case TT_GT: case TT_LT: case TT_GTE: case TT_LTE:
    case TT_BOR: case TT_BXOR: case TT_BAND:
    case TT_SHL: case TT_SHR:
    case TT_ADD: case TT_SUB:
    case TT_MUL: case TT_DIV: case TT_MOD:
    {
      expression_parser_push_op(ctx, (expr_op)tk->type, tk->len, tk->loc);

      ctx->state = EPS_UNARY;
    } break;

    case TT_R_BRACKET: {
      struct operation_item top_op;
      while ((top_op = ctx->operator_stack[ctx->operator_stack_ptr - 1]).op != MKR_LBRACKET
        && top_op.op != MKR_FUNCTION)
      {
        expression_parser_pop_op(ctx);
      }

      ctx->operator_stack_ptr -= 1;

      if (top_op.op == MKR_FUNCTION) {
        size_t stack_ptr = ctx->operand_stack_ptr;
        struct ast_expr *expr;
        struct ast_fn_call *fn_call;

        while (ctx->operand_stack[--ctx->operand_stack_ptr] != NULL) {}

        fn_call = arena_alloc(ctx->arena, sizeof(struct ast_fn_call));
        fn_call->args_len = stack_ptr - ctx->operand_stack_ptr - 1;

        fn_call->args = arena_alloc(ctx->arena, sizeof(struct ast_expr *) * fn_call->args_len);
        mempcpy(fn_call->args, &ctx->operand_stack[ctx->operand_stack_ptr + 1],
          sizeof(struct ast_expr *) * fn_call->args_len);

        fn_call->fn = ctx->operand_stack[--ctx->operand_stack_ptr];

        expr = arena_alloc(ctx->arena, sizeof(struct ast_expr));
        expr->type = TERM_FN_CALL;
        expr->as.fn_call = fn_call;
        expr->loc = fn_call->fn->loc;
        expr->len = tk->loc + tk->len - expr->loc;

        ctx->operand_stack[ctx->operand_stack_ptr++] = expr;
      }

      ctx->state = EPS_BINARY;
    } break;

    case TT_COLON: {
      struct operation_item top_op;
      while (ctx->operator_stack_ptr > 0
        && (top_op = ctx->operator_stack[ctx->operator_stack_ptr - 1]).op != MKR_LBRACKET
        && top_op.op != MKR_FUNCTION)
      {
        expression_parser_pop_op(ctx);
      }

      if (ctx->operator_stack_ptr == 0) {
        fprint_error(stderr, "use of comma outside of function arguments");
        fprint_error_ctx(stderr, ctx->parser->lex->src, 1, 0, tk->len, tk->loc, "here");
        fprint_note(stderr, "tuples do not exist in Jotunheim");

        return parser_error(ctx->parser);
      }

      if (top_op.op == MKR_LBRACKET) {
        fprint_error(stderr, "unclosed bracket");
        fprint_error_ctx(stderr, ctx->parser->lex->src, 1, 0, top_op.len, top_op.loc, "this bracket was never closed");
        fprintf(stderr, "\n");
        fprint_info_ctx(stderr, ctx->parser->lex->src, 1, 0, 1, tk->loc, "perhaps you forgot to add a ')' here");
        fprint_help(stderr, "expected a ')'");
        fprint_note(stderr, "tuples do not exist in Jotunheim");

        return parser_error(ctx->parser);
      }

      ctx->state = EPS_UNARY;
    } break;

    case TT_L_BRACKET: {
      ctx->operator_stack[ctx->operator_stack_ptr++] = (struct operation_item) {
        .op = MKR_FUNCTION,
        .len = tk->len,
        .loc = tk->loc,
      };

      ctx->operand_stack[ctx->operand_stack_ptr++] = NULL;

      ctx->state = EPS_UNARY;
    } break;

    default: {
      ctx->state = EPS_STOP;
    } break;
  }

  return true;
}

bool parser_parse_expression(struct parser *parser, struct ast_expr *expr) {
  struct token tk;
  struct expression_ctx ctx = {0};
  ctx.arena = parser->arena;

  ctx.parser = parser;
  
  while (lexer_peek(parser->lex, &tk)) {
    switch (ctx.state) {
      case EPS_UNARY: {
        if (!expression_parser_unary(&ctx, &tk))
          return false;

        if (ctx.state == EPS_STOP) {
          fprint_error(stderr, "got an unexpected %s token", token_type_tostring(tk.type));
          fprint_error_ctx(stderr, parser->lex->src, 1, 0, tk.len, tk.loc, "unexpected token");
          fprint_help(stderr, "expected a term");

          parser->error = true;
          return false;
        }
      } break;

      case EPS_BINARY: {
        if (!expression_parser_binary(&ctx, &tk))
          return false;
      } break;

      case EPS_STOP: {
        fprintf(stderr, "[%s:%d] the ES_STOP state should not be reachable in this state.", __FILE__, __LINE__);
      } break;
    }

    if (ctx.state == EPS_STOP) {
      goto finish_while;
    }

    lexer_next(parser->lex, &tk);
  }

  if (tk.type != TT_EOF)
    return parser_error(parser);

finish_while:
  while (ctx.operator_stack_ptr > 0) {
    struct operation_item top_op = ctx.operator_stack[ctx.operator_stack_ptr - 1];
    if (top_op.op == MKR_LBRACKET || top_op.op == MKR_FUNCTION)
      break;
    expression_parser_pop_op(&ctx);
  }

  if (ctx.operand_stack_ptr > 1) {
    const char *loc = NULL;
    size_t len;
    for (size_t i = 0; i < ctx.operator_stack_ptr; i++) {
      if (ctx.operator_stack[i].op == MKR_LBRACKET || ctx.operator_stack[i].op == MKR_FUNCTION) {
        loc = ctx.operator_stack[i].loc;
        len = ctx.operator_stack[i].len;
      }
    }

    if (loc != NULL) {
      fprint_error(stderr, "unclosed bracket");
      fprint_error_ctx(stderr, parser->lex->src, 1, 0, len, loc, "this bracket was never closed");
      fprintf(stderr, "\n");
      fprint_info_ctx(stderr, parser->lex->src, 1, 0, 1, tk.loc, "perhaps you forgot to add a ')' here");
      fprint_help(stderr, "expected a ')'");
    } else {
      printf("should be unreachable, stack size: %zu\n", ctx.operand_stack_ptr);
    }

    return false;
  }

  *expr = *ctx.operand_stack[--ctx.operand_stack_ptr];

  return true;
}
