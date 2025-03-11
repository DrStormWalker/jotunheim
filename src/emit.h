#ifndef EMIT_H
#define EMIT_H

#include <stdarg.h>
#include <stdio.h>
#include <stdbool.h>

#include "hashmap.h"
#include "ast.h"

struct string_buffer;

struct string_buffer *string_buffer_new();
void string_buffer_free(struct string_buffer *buf);
void string_buffer_dump_to_file(struct string_buffer *buf, FILE *fptr);
void string_buffer_dump_to_sb(struct string_buffer *src, struct string_buffer *dest);

void sb_printf(struct string_buffer *buf, const char *fmt, ...);
void vsb_printf(struct string_buffer *buf, const char *fmt, va_list vargs);

// typedef enum {
//   STATE_UNVISITED,
//   STATE_VISITING,
//   STATE_VISITED,
// } global_visit_state;

// struct global {
//   struct ident ident;
//   global_visit_state visit_state;
//   struct ast_const *c;
// };

// uint64_t global_hash(const struct global *g, uint64_t seed0, uint64_t seed1);
// int global_compare(const struct global *a, const struct global *b, void *udata);

typedef enum {
  VF_EMPTY = 0,
  VF_GLOBAL = 1 << 1,
  VF_VISITING = 1 << 2,
  VF_VISITED = 1 << 3,
  VF_LOAD = 1 << 4,
} variable_flags;

union variable_as {
  struct ast_const *global;
};

struct variable {
  variable_flags flags;
  struct ident ident;
  union variable_as as;
};

struct temporary {
  variable_flags flags;
  size_t id;
};

uint64_t variable_hash(const struct variable *v, uint64_t seed0, uint64_t seed1);
int variable_compare(const struct variable *a, const struct variable *b, void *udata);

struct scope {
  struct hashmap *members;
  struct scope *parent;
};

struct emit_ctx {
  const char *src;
  struct scope *global;
  struct scope *scope;
  struct string_buffer *out;
  struct temporary temp;
  size_t t, l;
};

struct scope *scope_new(struct scope *parent);
void scope_free(struct scope *scope);

struct variable *scope_set(struct scope *scope, struct variable *v);

bool scope_get_immediate_variable(struct scope *scope, struct emit_ctx *ctx, struct ident *ident, struct variable *v);
bool scope_get_variable(struct scope *scope, struct emit_ctx *ctx, struct ident *ident, struct variable *v);

bool emit_ast(struct string_buffer *out, const char *src, struct ast *ast);
bool emit_constant(struct string_buffer *out, struct emit_ctx *ctx, struct ast_const *c);
bool emit_proc(struct string_buffer *out, struct emit_ctx *ctx, struct ident *ident, struct ast_proc *proc);
bool emit_string_constant(struct string_buffer *out, struct emit_ctx *ctx, struct ident *ident, struct string *string);
bool emit_integer_constant(struct string_buffer *out, struct emit_ctx *ctx, struct ident *ident, int64_t *i);

bool emit_statement(struct string_buffer *out, struct emit_ctx *ctx, struct ast_stmt *stmt);
bool emit_expression(struct string_buffer *out, struct emit_ctx *ctx, struct ast_expr *expr);
bool emit_operation(struct string_buffer *out, struct emit_ctx *ctx, struct ast_operation *op);

bool emit_let_statement(struct string_buffer *out, struct emit_ctx *ctx, struct ast_assign *let);
bool emit_assign_statement(struct string_buffer *out, struct emit_ctx *ctx, struct ast_assign *let);

bool emit_if_statement(struct string_buffer *out, struct emit_ctx *ctx, struct ast_if *if_);

#endif /* EMIT_H */
