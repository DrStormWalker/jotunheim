#include "emit.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "ast.h"
#include "hashmap.h"
#include "error.h"
#include "expression.h"

struct string_buffer {
  size_t cap, len;
  char *buf;
};

struct string_buffer *string_buffer_new() {
  struct string_buffer *buf = malloc(sizeof(struct string_buffer));
  buf->len = 0;
  buf->cap = 16;
  buf->buf = malloc(16);
  buf->buf[0] = 0;

  return buf;
}

void string_buffer_free(struct string_buffer *buf) {
  if (buf->buf != NULL)
    free(buf->buf);
  
  free(buf);
}

void string_buffer_dump_to_file(struct string_buffer *buf, FILE *fptr) {
  fputs(buf->buf, fptr);
}

void string_buffer_dump_to_sb(struct string_buffer *src, struct string_buffer *dest) {
  if (src->len + dest->len + 1 >= dest->cap) {
    if (dest->cap == 0)
      dest->cap = 16;

    while (src->len + dest->len + 1 >= dest->cap)
      dest->cap *= 2;

    dest->buf = realloc(dest->buf, dest->cap);
  }

  memcpy(&dest->buf[dest->len], src->buf, src->len + 1);
  dest->len += src->len;
}

void sb_printf(struct string_buffer *buf, const char *fmt, ...) {
  va_list vargs;
  va_start(vargs, fmt);
  vsb_printf(buf, fmt, vargs);
  va_end(vargs);
}

void vsb_printf(struct string_buffer *buf, const char *fmt,  va_list vargs) {
  va_list vargs2;
  size_t required_len;
  va_copy(vargs2, vargs);
  required_len = vsnprintf(NULL, 0, fmt, vargs);

   if (buf->len + required_len + 1 >= buf->cap) {
     if (buf->cap == 0)
       buf->cap = 16;

     while (buf->len + required_len + 1 >= buf->cap)
       buf->cap *= 2;

     buf->buf = realloc(buf->buf, buf->cap);
  }

  vsprintf(&buf->buf[buf->len], fmt, vargs2);
  va_end(vargs2);
  buf->len += required_len;
}

// uint64_t global_hash(const struct global *g, uint64_t seed0, uint64_t seed1) {
//   return hashmap_sip(g->ident.chars, g->ident.len, seed0, seed1);
// }

// int global_compare(const struct global *a, const struct global *b, void *udata) {
//   int len_diff = a->ident.len - b->ident.len;

//   if (len_diff != 0)
//     return len_diff;

//   return strncmp(a->ident.chars, b->ident.chars, a->ident.len);
// }

uint64_t variable_hash(const struct variable *v, uint64_t seed0, uint64_t seed1) {
  return hashmap_sip(v->ident.chars, v->ident.len, seed0, seed1);
}

int variable_compare(const struct variable *a, const struct variable *b, void *udata) {
  int len_diff = a->ident.len - b->ident.len;

  if (len_diff != 0)
    return len_diff;

  return strncmp(a->ident.chars, b->ident.chars, a->ident.len);
}

struct scope *scope_new(struct scope *parent) {
  struct scope *scope = malloc(sizeof(struct scope));
  scope->parent = parent;
  scope->members = hashmap_new(sizeof(struct variable), 16, 0, 0,
    (uint64_t(*)(const void *, uint64_t, uint64_t))variable_hash,
    (int(*)(const void *, const void *, void *))variable_compare,
    NULL, NULL);

  return scope;
}

void scope_free(struct scope *scope) {
  hashmap_free(scope->members);
  free(scope);
}

struct variable *scope_set(struct scope *scope, struct variable *v) {
  return (struct variable *)hashmap_set(scope->members, (void *)v);
}

bool scope_get_immediate_variable(struct scope *scope, struct emit_ctx *ctx, struct ident *ident, struct variable *out) {
  struct variable *var, v;

  if (scope == NULL)
    return false;

  v.ident = *ident;

  var = (struct variable *)hashmap_get(scope->members, &v);
  if (var == NULL)
    return scope_get_variable(scope->parent, ctx, ident, out);


  if ((var->flags & VF_GLOBAL) && !(var->flags & VF_VISITED)) {
    if (var->flags & VF_VISITING)
      // Cycle
      return false;

    struct scope *scope = ctx->scope;
    ctx->scope = ctx->global;

    var->flags |= VF_VISITING;
    if (!emit_constant(ctx->out, ctx, var->as.global))
      return false;

    ctx->scope = scope;

    var->flags &= ~VF_VISITING;
    var->flags |= VF_VISITED;

    if (var->as.global->type == CONST_EXPR)
      var->flags |= VF_LOAD;
  }

  *out = *var;
  return true;
}

bool scope_get_variable(struct scope *scope, struct emit_ctx *ctx, struct ident *ident, struct variable *v) {
  if (scope == NULL)
    return false;

  if (!scope_get_immediate_variable(scope, ctx, ident, v))
    return scope_get_variable(scope->parent, ctx, ident, v);

  return true;
}

bool emit_ast(struct string_buffer *out, const char *src, struct ast *ast) {
  size_t i;
  struct variable *var, v = { .flags = VF_GLOBAL, };
  struct emit_ctx ctx;
  ctx.src = src;
  ctx.scope = scope_new(NULL);
  ctx.global = ctx.scope;
  ctx.out = out;
  ctx.t = 0;
  ctx.l = 0;
  ctx.temp = (struct temporary) {0};

  for (i = 0; i < ast->consts_len; i++) {
    v.as.global = &ast->consts[i];
    v.ident = ast->consts[i].ident;

    scope_set(ctx.scope, &v);
  }

  i = 0;

  while (hashmap_iter(ctx.scope->members, &i, (void **)&var)) {
    if (var->flags & VF_VISITED)
      continue;

    var->flags |= VF_VISITING;
    if (!emit_constant(out, &ctx, var->as.global)) {
      scope_free(ctx.scope);

      return false;
    }

    var->flags &= ~VF_VISITING;
    var->flags |= VF_VISITED;
  }

  scope_free(ctx.scope);

  return true;
}

bool emit_constant(struct string_buffer *out, struct emit_ctx *ctx, struct ast_const *c) {
  size_t t = ctx->t, l = ctx->l;
  struct string_buffer *buf = string_buffer_new();
  ctx->t = 0;
  ctx->l = 0;

  switch (c->type) {
    case CONST_PROC: {
      if (!emit_proc(buf, ctx, &c->ident, &c->as.proc)) {
        string_buffer_free(buf);
        return false;
      }
    } break;

    case CONST_PROC_DECLARATION: {
        
    } break;

    case CONST_EXPR: {
      if (c->as.expr->type != TERM_INT) {
        fprint_error(stderr, "expressions cannot be assigned to constants");
        fprint_error_ctx(stderr, ctx->src, 1, 0, c->as.expr->len, c->as.expr->loc, "this expression");
        fprint_help(stderr, "only literals can be assigned to constants");

        string_buffer_free(buf);
        return false;
      }

      if (!emit_integer_constant(buf, ctx, &c->ident, &c->as.expr->as.integer)) {
        string_buffer_free(buf);
        return false;
      }
    } break;

    case CONST_STRING: {
      if (!emit_string_constant(buf, ctx, &c->ident, &c->as.string)) {
        string_buffer_free(buf);
        return false;
      }
    } break;
  }

  string_buffer_dump_to_sb(buf, out);
  string_buffer_free(buf);

  ctx->t = t;
  ctx->l = l;

  return true;
}

bool emit_string_constant(struct string_buffer *out, struct emit_ctx *ctx, struct ident *ident, struct string *string) {
  sb_printf(out, "data $%.*s = { b \"%.*s\", b 0 }\n", ident->len, ident->chars, string->len, string->chars);

  return true;
}

bool emit_integer_constant(struct string_buffer *out, struct emit_ctx *ctx, struct ident *ident, int64_t *i) {
  sb_printf(out, "data $%.*s = { l %li }\n", ident->len, ident->chars, *i);

  return true;
}

bool emit_proc(struct string_buffer *out, struct emit_ctx *ctx, struct ident *ident, struct ast_proc *proc) {
  sb_printf(out, "export function l $%.*s ( ) {\n", ident->len, ident->chars);
  sb_printf(out, "@start\n");

  struct scope *scope = scope_new(ctx->scope);
  ctx->scope = scope;

  for (size_t i = 0; i < proc->stmts_len; i++) {
    if (!emit_statement(out, ctx, proc->stmts[i]))
      return false;
  }

  ctx->scope = scope->parent;
  scope_free(scope);

  sb_printf(out, "}\n");

  return true;
}

bool emit_statement(struct string_buffer *out, struct emit_ctx *ctx, struct ast_stmt *stmt) {
  switch (stmt->type) {
    case STMT_EXPR: {
      if (!emit_expression(out, ctx, stmt->as.expr))
        return false;
    } break;

    case STMT_RET: {
      struct temporary expr;
        
      if (stmt->as.ret == NULL) {
        sb_printf(out, "    ret\n");

        return true;
      }

      if (!emit_expression(out, ctx, stmt->as.ret))
        return false;

      expr = ctx->temp;

      if (expr.flags & VF_LOAD) {
        sb_printf(out, "    %%t_%lu =l loadl %%t_%lu\n", ctx->t, expr.id);
        expr.id = ctx->t++;
      }

      sb_printf(out, "    ret %%t_%lu\n", expr.id);
    } break;

    case STMT_LET: {
      if (!emit_let_statement(out, ctx, stmt->as.let))
        return false;
    } break;

    case STMT_ASSIGN: {
      if (!emit_assign_statement(out, ctx, stmt->as.assign))
        return false;
    } break;

    case STMT_IF: {
      if (!emit_if_statement(out, ctx, stmt->as.if_))
        return false;
    } break;
  }

  return true;
}

bool emit_expression(struct string_buffer *out, struct emit_ctx *ctx, struct ast_expr *expr) {
  switch (expr->type) {
    case TERM_INT: {
      sb_printf(out, "    %%t_%lu =l copy %li\n", ctx->t, expr->as.integer);

      ctx->temp.flags &= ~VF_LOAD;
      ctx->temp.id = ctx->t++;
    } break;

    case TERM_IDENT: {
      struct variable v;
      if (!scope_get_variable(ctx->scope, ctx, &expr->as.ident, &v))
        // variable not found
        return false;

      char scope = (v.flags & VF_GLOBAL) ? '$' : '%';

      // if (v.flags & VF_LOAD)
      //   sb_printf(out, "    %%t_%lu =l loadl %c%.*s\n", ctx->t++, scope, v.ident.len, v.ident.chars);
      // else
      //   sb_printf(out, "    %%t_%lu =l copy %c%.*s\n", ctx->t++, scope, v.ident.len, v.ident.chars);

      sb_printf(out, "    %%t_%lu =l copy %c%.*s\n", ctx->t, scope, v.ident.len, v.ident.chars);

      ctx->temp.flags &= ~VF_LOAD;
      ctx->temp.flags |= v.flags & VF_LOAD;
      ctx->temp.id = ctx->t++;
    } break;
      
    case TERM_FN_CALL: {
      struct string_buffer *buf = string_buffer_new();
      struct temporary fn, arg;

      if (!emit_expression(out, ctx, expr->as.fn_call->fn))
        return false;

      fn = ctx->temp;

      if (fn.flags & VF_LOAD) {
        sb_printf(out, "    %%t_%lu =l loadl %%t_%lu\n", ctx->t, fn.id);
        fn.id = ctx->t++;
      }

      for (size_t i = 0; i < expr->as.fn_call->args_len; i++) {
        if (!emit_expression(out, ctx, expr->as.fn_call->args[i]))
          return false;

        arg = ctx->temp;

        if (arg.flags & VF_LOAD) {
          sb_printf(out, "    %%t_%lu =l loadl %%t_%lu\n", ctx->t, arg.id);
          arg.id = ctx->t++;
        }

        sb_printf(buf, "l %%t_%lu, ", arg.id);
      }

      sb_printf(out, "    %%t_%lu =l call %%t_%lu ( ", ctx->t, fn.id);

      string_buffer_dump_to_sb(buf, out);
      string_buffer_free(buf);

      sb_printf(out, ")\n");

      ctx->temp.flags &= ~VF_LOAD;
      ctx->temp.id = ctx->t++;
    } break;

    case EXPR_OPERATION: {
      if (!emit_operation(out, ctx, &expr->as.op))
        return false;
    } break;
  }

  return true;
}

static const char *qbe_operations[] = {
  [OP_EQ] = "ceql", [OP_NEQ] = "cnel",
  [OP_GT] = "csgtl", [OP_LT] = "csltl", [OP_GTE] = "csgel", [OP_LTE] = "cslel",
  [OP_BOR] = "or", [OP_BXOR] = "xor", [OP_BAND] = "and",
  [OP_SHL] = "shl", [OP_SHR] = "shr",
  [OP_ADD] = "add", [OP_SUB] = "sub",
  [OP_MUL] = "mul", [OP_DIV] = "div", [OP_MOD] = "rem",
  [OP_NEG] = "neg",
};

bool emit_operation(struct string_buffer *out, struct emit_ctx *ctx, struct ast_operation *op) {
  struct temporary lhs, rhs;
  if (operator_is_unary(op->op)) {
    if (!emit_expression(out, ctx, op->lhs))
      return false;

    lhs = ctx->temp;

    if (lhs.flags & VF_LOAD) {
      sb_printf(out, "    %%t_%lu =l loadl %%t_%lu\n", ctx->t, lhs.id);
      lhs.id = ctx->t++;
    }
    
    sb_printf(out, "    %%t_%lu =l %s %%t_%lu\n", ctx->t++, qbe_operations[op->op], lhs.id);

    return true;
  }


  if (!emit_expression(out, ctx, op->lhs))
    return false;

  lhs = ctx->temp;

  if (lhs.flags & VF_LOAD) {
    sb_printf(out, "    %%t_%lu =l loadl %%t_%lu\n", ctx->t, lhs.id);
    lhs.id = ctx->t++;
  }

  if (!emit_expression(out, ctx, op->rhs))
    return false;

  rhs = ctx->temp;

  if (rhs.flags & VF_LOAD) {
    sb_printf(out, "    %%t_%lu =l loadl %%t_%lu\n", ctx->t, rhs.id);
    rhs.id = ctx->t++;
  }

  sb_printf(out, "    %%t_%lu =l %s %%t_%lu, %%t_%lu\n", ctx->t, qbe_operations[op->op], lhs.id, rhs.id);

  ctx->temp.id = ctx->t++;
  ctx->temp.flags &= ~VF_LOAD;
  
  return true;
}

bool emit_let_statement(struct string_buffer *out, struct emit_ctx *ctx, struct ast_assign *let) {
  struct variable v;
  if (scope_get_immediate_variable(ctx->scope, ctx, &let->ident, &v))
    // Redefinition
    return false;
  
  if (!emit_expression(out, ctx, let->expr))
    return false;

  v.ident = let->ident;
  v.flags = VF_LOAD;

  scope_set(ctx->scope, &v);

  sb_printf(out, "    %%%.*s =l alloc8 8\n", let->ident.len, let->ident.chars);
  sb_printf(out, "    storel %%t_%lu, %%%.*s\n", ctx->temp.id, let->ident.len, let->ident.chars);
  
  return true;
}

bool emit_assign_statement(struct string_buffer *out, struct emit_ctx *ctx, struct ast_assign *let) {
  struct variable v;
  if (!scope_get_immediate_variable(ctx->scope, ctx, &let->ident, &v))
    // Not defined
    return false;
  
  if (!emit_expression(out, ctx, let->expr))
    return false;

  v.ident = let->ident;
  v.flags = VF_LOAD;

  scope_set(ctx->scope, &v);

  sb_printf(out, "    storel %%t_%lu, %%%.*s\n", ctx->t - 1, let->ident.len, let->ident.chars);
  
  return true;
}

bool emit_if_statement(struct string_buffer *out, struct emit_ctx *ctx, struct ast_if *if_) {
  size_t true_, false_, final;
  struct temporary temp;
  struct ast_if_branch *branch;

  final = ctx->l++;

  for (size_t i = 0; i < if_->branches_len; i++) {
    branch = &if_->branches[i];

    if (!emit_expression(out, ctx, branch->cond))
      return false;

    temp = ctx->temp;

    if (temp.flags & VF_LOAD) {
      sb_printf(out, "    %%t_%lu =l loadl %%t_%lu\n", ctx->t, temp.id);
      temp.id = ctx->t++;
    }

    true_ = ctx->l++;
    false_ = ctx->l++;

    sb_printf(out, "    jnz %%t_%lu, @L_%lu, @L_%lu\n", temp.id, true_, false_);
    sb_printf(out, "@L_%lu\n", true_);

    struct scope *scope = scope_new(ctx->scope);
    ctx->scope = scope;

    for (size_t i = 0; i < branch->stmts_len; i++) {
      if (!emit_statement(out, ctx, branch->stmts[i]))
        return false;
    }

    ctx->scope = scope->parent;
    scope_free(scope);

    sb_printf(out, "    jmp @L_%lu\n", final);
    sb_printf(out, "@L_%lu\n", false_);
  }

  struct scope *scope = scope_new(ctx->scope);
  ctx->scope = scope;

  for (size_t i = 0; i < if_->else_stmts_len; i++) {
    if (!emit_statement(out, ctx, if_->else_stmts[i]))
      return false;
  }

  ctx->scope = scope->parent;
  scope_free(scope);

  sb_printf(out, "    jmp @L_%lu\n", final);
  sb_printf(out, "@L_%lu\n", final);

  return true;
}

