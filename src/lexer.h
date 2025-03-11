#ifndef LEXER_H
#define LEXER_H

#include <stddef.h>
#include <stdbool.h>

typedef enum {
  /* This must begin with keywords. */
  TT_PROC,
  TT_RETURN,
  TT_IF,
  TT_ELSE,

  NUM_KEYWORDS,

  /* Other tokens, can follow here. */
  TT_IDENT,
  TT_INTEGER,
  TT_STRING,
  TT_DOUBLE_COLON,
  TT_COLON_EQUALS,
  TT_EQUALS,
  TT_SEMI_COLON,
  TT_COLON,
  TT_L_BRACKET,
  TT_R_BRACKET,
  TT_L_CURLY,
  TT_R_CURLY,

  /* Operators */
  TT_EQ, TT_NEQ,
  TT_GT, TT_LT, TT_GTE, TT_LTE,
  TT_BOR, TT_BXOR, TT_BAND,
  TT_SHL, TT_SHR,
  TT_ADD, TT_SUB,
  TT_MUL, TT_DIV, TT_MOD,

  /* Leave as final */
  TT_EOF
} token_type;

#define IS_KEYWORD(x) (x < NUM_KEYWORDS)

static const char *keywords[] = {
  "proc",
  "return",
  "if",
  "else",
};
static size_t keywords_len = sizeof(keywords) / sizeof(*keywords);

const char *token_type_tostring(token_type type);

struct token {
  token_type type;
  size_t len;
  const char *loc;
};

struct lexer {
  const char *src;
  const char *loc;

  size_t num_peeked;
  struct token peeked[2];
  struct token history[2];
};

struct lexer lexer_new(const char *src);

bool lexer_next(struct lexer *lex, struct token *tk);
bool lexer_peek(struct lexer *lex, struct token *tk);
bool lexer_peek_n(struct lexer *lex, size_t n, struct token *tk);

#endif /* LEXER_H */
