#include "parser.h"

#include <stdlib.h>

#include "ast.h"
#include "lexer.h"
#include "vector.h"
#include "error.h"
#include "arena.h"

static inline bool parser_error(struct parser *parser) {
  parser->error = true;
  return false;
}

struct parser parser_new(struct arena *arena, struct lexer *lex) {
  struct parser parser = {
    .error = false,
    .arena = arena,
    .lex = lex,
  };

  return parser;
}

bool parser_expect(struct parser *parser, token_type tt, struct token *tk) {
  if (!lexer_next(parser->lex, tk)) {
    parser->error = tk->type != TT_EOF;
    return false;
  }

  return tk->type == tt;
}

bool parser_parse_ast(struct parser *parser, struct ast *ast) {
  struct token tk;
  struct ast_const c;
  struct vector *vec = vector_new(sizeof(struct ast_const), 16, NULL);

  while (true && lexer_peek(parser->lex, &tk)) {
    if (!parser_parse_const(parser, &c))
      return false;

    vector_push(vec, &c);
  }

  ast->consts_len = vector_into_inner(vec, (void **)&ast->consts);

  if (tk.type != TT_EOF) {
    return false;
  }

  return true;
}

bool parser_parse_const(struct parser *parser, struct ast_const *c) {
  struct token tk;

  if (!parser_expect(parser, TT_IDENT, &tk)) {
    if (parser->error)
      return false;

    if (IS_KEYWORD(tk.type)) {
      fprint_error(stderr, "keywords cannot be used as identifiers", token_type_tostring(tk.type));
      fprint_error_ctx(stderr, parser->lex->src, 1, 0, tk.len, tk.loc, "this is a keyword");
    } else if (tk.type == TT_EOF) {
      fprint_error(stderr, "input unexpectedly ended");
      fprint_info_ctx(stderr, parser->lex->src, 1, 1, 1, parser->lex->history[1].loc, "here");
      fprint_help(stderr, "expected an identifier");
    } else {
      fprint_error(stderr, "got an unexpected %s token", token_type_tostring(tk.type));
      fprint_error_ctx(stderr, parser->lex->src, 1, 0, tk.len, tk.loc, "unexpected token");
      fprint_help(stderr, "expected an identifier");
    }

    return parser_error(parser);
  }

  c->ident.len = tk.len;
  c->ident.chars = tk.loc;

  if (!parser_expect(parser, TT_DOUBLE_COLON, &tk)) {
    if (parser->error)
      return false;

    if (tk.type == TT_EOF) {
      fprint_error(stderr, "input unexpectedly ended");
      fprint_info_ctx(stderr, parser->lex->src, 1, 1, 1, parser->lex->history[1].loc, "here");
      fprint_help(stderr, "expected '::'");
    } else {
      fprint_error(stderr, "got an unexpected %s token", token_type_tostring(tk.type));
      fprint_error_ctx(stderr, parser->lex->src, 1, 0, tk.len, tk.loc, "unexpected token");
      fprint_help(stderr, "expected '::'");
    }

    return parser_error(parser);
  }

  if (!lexer_peek(parser->lex, &tk)) {
    if (tk.type != TT_EOF)
      return parser_error(parser);

    fprint_error(stderr, "input unexpectedly ended");
    fprint_info_ctx(stderr, parser->lex->src, 1, 1, 1, parser->lex->history[1].loc, "here");
    fprint_help(stderr, "expected an expression or procedure definition");
    
    return parser_error(parser);
  }

  switch (tk.type) {
    case TT_PROC: {
      c->type = CONST_PROC;
      return parser_parse_proc(parser, c);
    } break;

    case TT_STRING: {
      c->type = CONST_STRING;
      c->as.string.len = tk.len - 2;
      c->as.string.chars = tk.loc + 1;

      lexer_next(parser->lex, &tk);
    } break;

    case TT_INTEGER:
    case TT_IDENT: {
      c->type = CONST_EXPR;
      c->as.expr = arena_alloc(parser->arena, sizeof(struct ast_expr));
      if (!parser_parse_expression(parser, c->as.expr))
        return parser_error(parser);
    } break;

    default: {
      fprint_error(stderr, "got an unexpected %s token", token_type_tostring(tk.type));
      fprint_error_ctx(stderr, parser->lex->src, 1, 0, tk.len, tk.loc, "unexpected token");
      fprint_help(stderr, "expected a procedure or expression definition");

      return parser_error(parser);
    } break;
  }

  if (!parser_expect(parser, TT_SEMI_COLON, &tk)) {
    if (parser->error)
      return parser_error(parser);

    if (tk.type == TT_EOF) {
      fprint_error(stderr, "input unexpectedly ended");
      fprint_info_ctx(stderr, parser->lex->src, 1, 1, 1, parser->lex->history[1].loc, "here");
      fprintf(stderr, "\n");
      fprint_info_ctx(stderr, parser->lex->src, 1, 1, 1, parser->lex->history[1].loc, "perhaps you forgot to add a ';' here");
      fprint_help(stderr, "expected ';'");
    } else {
      fprint_error(stderr, "got an unexpected %s token", token_type_tostring(tk.type));
      fprint_error_ctx(stderr, parser->lex->src, 1, 0, tk.len, tk.loc, "unexpected token");
      fprintf(stderr, "\n");
      fprint_info_ctx(stderr, parser->lex->src, 1, 1, 1, parser->lex->history[1].loc, "perhaps you forgot to add a ';' here");
      fprint_help(stderr, "expected ';'");
    }

    return parser_error(parser);
  }

  return true;
}

bool parser_parse_proc(struct parser *parser, struct ast_const *c) {
  struct token tk;
  struct ast_proc proc;
  struct ast_stmt *stmt, **stmts_vec;
  struct vector *stmts;

  if (!parser_expect(parser, TT_PROC, &tk)) {
    if (parser->error)
      return false;

    if (tk.type == TT_EOF) {
      fprint_error(stderr, "input unexpectedly ended");
      fprint_info_ctx(stderr, parser->lex->src, 1, 1, 1, parser->lex->history[1].loc, "here");
      fprint_help(stderr, "expected '('");
    } else {
      fprint_error(stderr, "got an unexpected %s token", token_type_tostring(tk.type));
      fprint_error_ctx(stderr, parser->lex->src, 1, 0, tk.len, tk.loc, "unexpected token");
      fprint_help(stderr, "expected proc");
    }

    return parser_error(parser);
  }

  if (!parser_expect(parser, TT_L_BRACKET, &tk)) {
    if (parser->error)
      return false;

    if (tk.type == TT_EOF) {
      fprint_error(stderr, "input unexpectedly ended");
      fprint_info_ctx(stderr, parser->lex->src, 1, 1, 1, parser->lex->history[1].loc, "here");
      fprint_help(stderr, "expected '('");
    } else {
      fprint_error(stderr, "got an unexpected %s token", token_type_tostring(tk.type));
      fprint_error_ctx(stderr, parser->lex->src, 1, 0, tk.len, tk.loc, "unexpected token");
      fprint_help(stderr, "expected '('");
    }

    return parser_error(parser);
  }

  if (!parser_expect(parser, TT_R_BRACKET, &tk)) {
    if (parser->error)
      return false;

    if (tk.type == TT_EOF) {
      fprint_error(stderr, "input unexpectedly ended");
      fprint_info_ctx(stderr, parser->lex->src, 1, 1, 1, parser->lex->history[1].loc, "here");
      fprint_help(stderr, "expected ')'");
    } else {
      fprint_error(stderr, "got an unexpected %s token", token_type_tostring(tk.type));
      fprint_error_ctx(stderr, parser->lex->src, 1, 0, tk.len, tk.loc, "unexpected token");
      fprint_help(stderr, "expected ')'");
      fprint_note(stderr, "For the time being, procedures can take no arguments");
    }

    return parser_error(parser);
  }

  if (!lexer_next(parser->lex, &tk)) {
    if (tk.type != TT_EOF)
      return false;

    fprint_error(stderr, "input unexpectedly ended");
    fprint_info_ctx(stderr, parser->lex->src, 1, 1, 1, parser->lex->history[1].loc, "here");
    fprint_help(stderr, "expected '{' or ';'");

    return parser_error(parser);
  }

  if (tk.type == TT_SEMI_COLON) {
    c->type = CONST_PROC_DECLARATION;

    return true;
  } else if (tk.type != TT_L_CURLY) {
    fprint_error(stderr, "got an unexpected %s token", token_type_tostring(tk.type));
    fprint_error_ctx(stderr, parser->lex->src, 1, 0, tk.len, tk.loc, "unexpected token");
    fprint_help(stderr, "expected '{'");
    fprint_note(stderr, "For the time being, procedures cannot define their return type, it is assumed to be i64");

    return parser_error(parser);
  }

  stmts = vector_new(sizeof(struct ast_stmt *), 16, NULL);

  while (lexer_peek(parser->lex, &tk) && tk.type != TT_R_CURLY) {
    stmt = arena_alloc(parser->arena, sizeof(struct ast_stmt));
    if (!parser_parse_stmt(parser, stmt))
      return false;

    vector_push(stmts, &stmt);
  }

  proc.stmts_len = vector_into_inner(stmts, (void **)&stmts_vec);
  proc.stmts = arena_alloc(parser->arena, sizeof(struct ast_stmt *) * proc.stmts_len);

  for (size_t i = 0; i < proc.stmts_len; i++) {
    proc.stmts[i] = stmts_vec[i];
  }
  c->type = CONST_PROC;
  c->as.proc = proc;

  free(stmts_vec);

  // if (tk.type != TT_R_CURLY) {
  //   if (tk.type != TT_EOF)
  //     return parser_error(parser);

  //   fprint_error(stderr, "input unexpectedly ended");
  //   fprint_info_ctx(stderr, parser->lex->src, 1, 1, 1, parser->lex->history[1].loc, "here");
  //   fprint_help(stderr, "expected '}'");
    
  //   return parser_error(parser);
  // }

  if (!parser_expect(parser, TT_R_CURLY, &tk)) {
    if (parser->error)
      return false;

    if (tk.type == TT_EOF) {
      fprint_error(stderr, "input unexpectedly ended");
      fprint_info_ctx(stderr, parser->lex->src, 1, 1, 1, parser->lex->history[1].loc, "here");
      fprint_help(stderr, "expected '}'");
    } else {
      fprint_error(stderr, "got an unexpected %s token", token_type_tostring(tk.type));
      fprint_error_ctx(stderr, parser->lex->src, 1, 0, tk.len, tk.loc, "unexpected token");
      fprint_help(stderr, "expected '}'");
    }

    return parser_error(parser);
  }

  return true;
}

bool parser_parse_stmt(struct parser *parser, struct ast_stmt *stmt) {
  struct token tk;
  
  if (!lexer_peek(parser->lex, &tk)) {
    if (tk.type != TT_EOF)
      return parser_error(parser);

    fprint_error(stderr, "input unexpectedly ended");
    fprint_info_ctx(stderr, parser->lex->src, 1, 1, 1, parser->lex->history[1].loc, "here");
    fprint_help(stderr, "expected a statement");

    return parser_error(parser);
  }

  switch (tk.type) {
    case TT_RETURN: {
      stmt->type = STMT_RET;
      lexer_next(parser->lex, &tk);
        
      if (!lexer_peek(parser->lex, &tk)) {
        if (tk.type != TT_EOF)
          return parser_error(parser);

        fprint_error(stderr, "input unexpectedly ended");
        fprint_info_ctx(stderr, parser->lex->src, 1, 1, 1, parser->lex->history[1].loc, "here");
        fprint_help(stderr, "expected an expression or ';'");

        return parser_error(parser);
      }

      if (tk.type == TT_SEMI_COLON) {
        lexer_next(parser->lex, &tk);

        stmt->as.ret = NULL;
        return true;
      }

      stmt->as.ret = arena_alloc(parser->arena, sizeof(struct ast_expr));
      if (!parser_parse_expression(parser, stmt->as.ret))
        return parser_error(parser);
    } break;

    case TT_IF: {
      stmt->type = STMT_IF;
      stmt->as.if_ = arena_alloc(parser->arena, sizeof(struct ast_if));
      if (!parser_parse_if(parser, stmt->as.if_))
        return false;

      goto no_semi_colon;
    } break;

    case TT_IDENT: {
      if (!lexer_peek_n(parser->lex, 2, &tk)) {
        if (tk.type == TT_EOF)
          goto fallthrough;

        return parser_error(parser);
      }

      switch (tk.type) {
        case TT_COLON_EQUALS: {
          stmt->type = STMT_LET;
          stmt->as.let = arena_alloc(parser->arena, sizeof(struct ast_assign));
          if (!parser_parse_let(parser, stmt->as.let))
            return parser_error(parser);
        } break;

        case TT_EQUALS: {
          stmt->type = STMT_ASSIGN;
          stmt->as.assign = arena_alloc(parser->arena, sizeof(struct ast_assign));
          if (!parser_parse_assign(parser, stmt->as.assign))
            return parser_error(parser);
        } break;

        default: {
          goto fallthrough;
        } break;
      }
    } break;

    default: {
fallthrough:
      stmt->type = STMT_EXPR;
      stmt->as.expr = arena_alloc(parser->arena, sizeof(struct ast_expr));
      if (!parser_parse_expression(parser, stmt->as.expr))
        return parser_error(parser);
    } break;
  }

  if (!parser_expect(parser, TT_SEMI_COLON, &tk)) {
    if (parser->error)
      return parser_error(parser);

    if (tk.type == TT_EOF) {
      fprint_error(stderr, "input unexpectedly ended");
      fprint_info_ctx(stderr, parser->lex->src, 1, 1, 1, parser->lex->history[1].loc, "here");
      fprintf(stderr, "\n");
      fprint_info_ctx(stderr, parser->lex->src, 1, 1, 1, parser->lex->history[1].loc, "perhaps you forgot to add a ';' here");
      fprint_help(stderr, "expected ';'");
    } else {
      fprint_error(stderr, "got an unexpected %s token.", token_type_tostring(tk.type));
      fprint_error_ctx(stderr, parser->lex->src, 1, 0, tk.len, tk.loc, "unexpected token");
      fprintf(stderr, "\n");
      fprint_info_ctx(stderr, parser->lex->src, 1, 1, 1, parser->lex->history[1].loc, "perhaps you forgot to add a ';' here");
      fprint_help(stderr, "expected ';'");
    }

    return parser_error(parser);
  }
no_semi_colon:

  return true;
}

bool parser_parse_let(struct parser *parser, struct ast_assign *assign) {
  struct token tk;

  if (!parser_expect(parser, TT_IDENT, &tk)) {
    if (parser->error)
      return parser_error(parser);

    // TODO: Add error message

    return parser_error(parser);
  }

  assign->ident.len = tk.len;
  assign->ident.chars = tk.loc;

  if (!parser_expect(parser, TT_COLON_EQUALS, &tk)) {
    if (parser->error)
      return parser_error(parser);

    // TODO: Add error message

    return parser_error(parser);
  }

  assign->expr = arena_alloc(parser->arena, sizeof(struct ast_expr));
  if (!parser_parse_expression(parser, assign->expr))
    return false;
  
  return true;
}

bool parser_parse_assign(struct parser *parser, struct ast_assign *assign) {
  struct token tk;

  if (!parser_expect(parser, TT_IDENT, &tk)) {
    if (parser->error)
      return parser_error(parser);

    // TODO: Add error message

    return parser_error(parser);
  }

  assign->ident.len = tk.len;
  assign->ident.chars = tk.loc;

  if (!parser_expect(parser, TT_EQUALS, &tk)) {
    if (parser->error)
      return parser_error(parser);

    // TODO: Add error message

    return parser_error(parser);
  }

  assign->expr = arena_alloc(parser->arena, sizeof(struct ast_expr));
  if (!parser_parse_expression(parser, assign->expr))
    return parser_error(parser);
  
  return true;
}

bool parser_parse_branch(struct parser *parser, struct ast_if_branch *branch) {
  struct token tk;
  struct vector *stmts_vec;
  struct ast_stmt *stmt, **stmts_inner;

  if (!parser_expect(parser, TT_IF, &tk)) {
    if (parser->error)
      return parser_error(parser);

    // TODO: Add error message
  
    return parser_error(parser);
  }

  branch->cond = arena_alloc(parser->arena, sizeof(struct ast_expr));
  if (!parser_parse_expression(parser, branch->cond))
    return parser_error(parser);

  if (!parser_expect(parser, TT_L_CURLY, &tk)) {
    if (parser->error)
      return parser_error(parser);

    if (tk.type == TT_EOF) {
      fprint_error(stderr, "input unexpectedly ended");
      fprint_info_ctx(stderr, parser->lex->src, 1, 1, 1, parser->lex->history[1].loc, "here");
      fprint_help(stderr, "expected '{'");
    } else {
      fprint_error(stderr, "got an unexpected %s token", token_type_tostring(tk.type));
      fprint_error_ctx(stderr, parser->lex->src, 1, 0, tk.len, tk.loc, "unexpected token");
      fprint_help(stderr, "expected '{'");
    }

    return parser_error(parser);
  }

  stmts_vec = vector_new(sizeof(struct ast_stmt *), 16, NULL);

  while (lexer_peek(parser->lex, &tk) && tk.type != TT_R_CURLY) {
    stmt = arena_alloc(parser->arena, sizeof(struct ast_stmt));
    if (!parser_parse_stmt(parser, stmt))
      return false;

    vector_push(stmts_vec, &stmt);
  }

  branch->stmts_len = vector_into_inner(stmts_vec, (void **)&stmts_inner);
  branch->stmts = arena_alloc(parser->arena, sizeof(struct ast_stmt *) * branch->stmts_len);

  for (size_t i = 0; i < branch->stmts_len; i++) {
    branch->stmts[i] = stmts_inner[i];
  }

  free(stmts_inner);

  if (!parser_expect(parser, TT_R_CURLY, &tk)) {
    if (parser->error)
      return parser_error(parser);

    if (tk.type == TT_EOF) {
      fprint_error(stderr, "input unexpectedly ended");
      fprint_info_ctx(stderr, parser->lex->src, 1, 1, 1, parser->lex->history[1].loc, "here");
      fprint_help(stderr, "expected '}'");
    } else {
      fprint_error(stderr, "got an unexpected %s token", token_type_tostring(tk.type));
      fprint_error_ctx(stderr, parser->lex->src, 1, 0, tk.len, tk.loc, "unexpected token");
      fprint_help(stderr, "expected '}'");
    }

    return parser_error(parser);
  }

  return true;
}

bool parser_parse_if(struct parser *parser, struct ast_if *if_) {
  struct token tk;
  struct ast_if_branch branch, *branches_inner;
  struct vector *branches_vec;

  if_->else_stmts_len = 0;
  if_->else_stmts = NULL;

  branches_vec = vector_new(sizeof(struct ast_if_branch), 16, NULL);

  if (!parser_parse_branch(parser, &branch))
    return parser_error(parser);

  vector_push(branches_vec, &branch);

  while (1) {
    if (!lexer_peek(parser->lex, &tk)) {
      if (tk.type != TT_EOF)
        return parser_error(parser);

      // TODO: Add error message
      
      return parser_error(parser);
    }

    if (tk.type != TT_ELSE)
      break;

    lexer_next(parser->lex, &tk);

    if (!lexer_peek(parser->lex, &tk)) {
      if (tk.type != TT_EOF)
        return parser_error(parser);

      // TODO: Add error message
      
      return parser_error(parser);
    }

    if (tk.type == TT_IF) {
      if (!parser_parse_branch(parser, &branch))
        return parser_error(parser);

      vector_push(branches_vec, &branch);

      continue;
    }

    if (!parser_expect(parser, TT_L_CURLY, &tk)) {
      if (parser->error)
        return parser_error(parser);

      if (tk.type == TT_EOF) {
        fprint_error(stderr, "input unexpectedly ended");
        fprint_info_ctx(stderr, parser->lex->src, 1, 1, 1, parser->lex->history[1].loc, "here");
        fprint_help(stderr, "expected '{'");
      } else {
        fprint_error(stderr, "got an unexpected %s token", token_type_tostring(tk.type));
        fprint_error_ctx(stderr, parser->lex->src, 1, 0, tk.len, tk.loc, "unexpected token");
        fprint_help(stderr, "expected '{'");
      }

      return parser_error(parser);
    }

    struct vector *stmts_vec;
    struct ast_stmt *stmt, **stmts_inner;

    stmts_vec = vector_new(sizeof(struct ast_stmt *), 16, NULL);

    while (lexer_peek(parser->lex, &tk) && tk.type != TT_R_CURLY) {
      stmt = arena_alloc(parser->arena, sizeof(struct ast_stmt));
      if (!parser_parse_stmt(parser, stmt))
        return false;

      vector_push(stmts_vec, &stmt);
    }

    if_->else_stmts_len = vector_into_inner(stmts_vec, (void **)&stmts_inner);
    if_->else_stmts = arena_alloc(parser->arena, sizeof(struct ast_stmt *) * if_->else_stmts_len);

    for (size_t i = 0; i < if_->else_stmts_len; i++) {
      if_->else_stmts[i] = stmts_inner[i];
    }

    free(stmts_inner);

    if (!parser_expect(parser, TT_R_CURLY, &tk)) {
      if (parser->error)
        return parser_error(parser);

      if (tk.type == TT_EOF) {
        fprint_error(stderr, "input unexpectedly ended");
        fprint_info_ctx(stderr, parser->lex->src, 1, 1, 1, parser->lex->history[1].loc, "here");
        fprint_help(stderr, "expected '}'");
      } else {
        fprint_error(stderr, "got an unexpected %s token", token_type_tostring(tk.type));
        fprint_error_ctx(stderr, parser->lex->src, 1, 0, tk.len, tk.loc, "unexpected token");
        fprint_help(stderr, "expected '}'");
      }

      return parser_error(parser);
    }

    break;
  }

  if_->branches_len = vector_into_inner(branches_vec, (void **)&branches_inner);
  if_->branches = arena_alloc(parser->arena, sizeof(struct ast_if_branch) * if_->branches_len);

  for (size_t i = 0; i < if_->branches_len; i++) {
    if_->branches[i] = branches_inner[i];
  }

  free(branches_inner);

  return true;
}

