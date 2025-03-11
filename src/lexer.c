#include "lexer.h"

#include <stdio.h>
#include <ctype.h>
#include <stddef.h>
#include <string.h>
#include <stdbool.h>

#include "error.h"

static const char *token_type_strings[] = {
  "proc", "return", "if", "else",
  "",
  "identifier",
  "integer", "string",
  "'::'", "':='",
  "'='",
  "';'",
  "','",
  "'('", "')'", "'{'", "'}'",
  "'=='", "'!='",
  "'>'", "'<'", "'>='", "'<='",
  "'|'", "'^'", "'&'",
  "'<<'", "'>>'",
  "'+'", "'-'",
  "'*'", "'/'", "'%'",
  "eof",
};
const char *token_type_tostring(token_type type) {
  return token_type_strings[type];
}

int qualify_keyword(size_t word_len, const char *word) {
  /* Iterate through each keyword, and check if the word matches. */
  for (int i = 0; i < keywords_len; i++) {
    /* Compare null terminated string with string+length */
    if (strncmp(keywords[i], word, word_len) == 0
      && keywords[i][word_len] == 0)
    {
      return i;
    }
  }

  /* No keyword found */
  return -1;
}

struct lexer lexer_new(const char *src) {
  struct lexer l = {
    .src = src,
    .loc = src,
  };

  return l;
}

static bool lexer_next_no_peek(struct lexer *lex, struct token *tk) {
  /* Skip whitepscae. */
  while (isspace(*lex->loc)) {
    lex->loc++;
  }

  tk->loc = lex->loc;

  if (*lex->loc == 0) {
    /* Successfully read all tokens */
    tk->type = TT_EOF;
    return false;
  }
  
  /* Is token an identifier or a keyword? */
  if (isalpha(*lex->loc) || *lex->loc == '_') {
    while (isalnum(*lex->loc) || *lex->loc == '_') {
      lex->loc++;
    }

    tk->len = lex->loc - tk->loc;

    /* Is token a keyword? */
    int keyword = qualify_keyword(tk->len, tk->loc);
    tk->type = keyword >= 0 ? (token_type)keyword : TT_IDENT;

    return true;
  }

  /* Is token an number? (only integers exist at the moment) */
  if (isdigit(*lex->loc)) {
    while (isdigit(*lex->loc)) {
      lex->loc++;
    }

    if (isalpha(*lex->loc) || *lex->loc == '_') {
      /* Probably using an identifier starting with a digit */
      while (isalnum(*lex->loc) || *lex->loc == '_') {
        lex->loc++;
      }

      tk->len = lex->loc - tk->loc;

      fprint_error(stderr, "identifiers cannot start with a digit");
      fprint_error_ctx(stderr, lex->src, 1, 0, tk->len, tk->loc, "this identifier");
      fprint_help(stderr, "identifers can only start with 'a-z', 'A-Z', or '_'");

      return false;
    }

    tk->len = lex->loc - tk->loc;
    tk->type = TT_INTEGER;
    return true;
  }

  tk->len = 1;

  /* Token is something else. */
  switch (*(lex->loc++)) {
    case '"': {
      while (*(lex->loc++) != '"') {
        if (*lex->loc == '\\')
          lex->loc++;
      }

      tk->len = lex->loc - tk->loc;
      tk->type = TT_STRING;
    } break;
    
    case ':': {
      if (*lex->loc == ':') {
        tk->type = TT_DOUBLE_COLON;
      } else if (*lex->loc == '=') {
        tk->type = TT_COLON_EQUALS;
      } else {
        fprint_error(stderr, "use of invalid token");
        fprint_error_ctx(stderr, lex->src, 1, 0, 2, tk->loc, "here");
        fprint_help(stderr, "perhaps you meant to use '::' or ':='");

        return false;
      }

      tk->len = 2;
      lex->loc++;
    } break;

    case '=': {
      if (*lex->loc == '=') {
        lex->loc++;
        tk->type = TT_EQ;
        tk->len = 2;
      } else {
        tk->type = TT_EQUALS;
      }
    } break;

    case '!': {
      if (*(lex->loc++) != '=') {
        fprint_error(stderr, "use of invalid token");
        fprint_error_ctx(stderr, lex->src, 1, 0, 2, tk->loc, "here");
        fprint_help(stderr, "perhaps you meant to use '!='");

        return false;
      }

      tk->type = TT_NEQ;
      tk->len = 2;
    } break;

    case '<': {
      if (*lex->loc == '=') {
        lex->loc++;
        tk->type = TT_LTE;
        tk->len = 2;
      } else {
        tk->type = TT_LT;
      }
    } break;

    case '>': {
      if (*lex->loc == '=') {
        lex->loc++;
        tk->type = TT_GTE;
        tk->len = 2;
      } else {
        tk->type = TT_GT;
      }
    } break;

    case ';': { tk->type = TT_SEMI_COLON; } break;
    case ',': { tk->type = TT_COLON; } break;
    case '(': { tk->type = TT_L_BRACKET; } break;
    case ')': { tk->type = TT_R_BRACKET; } break;
    case '{': { tk->type = TT_L_CURLY; } break;
    case '}': { tk->type = TT_R_CURLY; } break;

    case '+': { tk->type = TT_ADD; } break;
    case '-': { tk->type = TT_SUB; } break;
    case '*': { tk->type = TT_MUL; } break;

    default: {
      fprint_error(stderr, "use of invalid token");
      fprint_error_ctx(stderr, lex->src, 1, 0, 1, tk->loc, "here");
      fprint_note(stderr, "only identifiers, keywords and integers have been implemented so far");

      return false;
    } break;
  }

  return true;
}

static bool lexer_next_no_history(struct lexer *lex, struct token *tk) {
  /* Token was previously peeked. */
  if (lex->num_peeked > 0) {
    *tk = lex->peeked[0];

    for (size_t i = 1; i < lex->num_peeked; i++) {
      lex->peeked[i - 1] = lex->peeked[i];
    }

    lex->num_peeked--;

    return true;
  }

  return lexer_next_no_peek(lex, tk);
}

bool lexer_next(struct lexer *lex, struct token *tk) {
  if (!lexer_next_no_history(lex, tk) && tk->type != TT_EOF)
    return false;

  for (size_t i = 2 - 1; i >= 1; i--) {
    lex->history[i] = lex->history[i - 1];
  }

  lex->history[0] = *tk;

  return tk->type != TT_EOF;
}

bool lexer_peek(struct lexer *lex, struct token *tk) {
  /* Has a token been peeked yet? */
  if (lex->num_peeked < 1) {
    /* If not, get a new token. */
    if (!lexer_next_no_peek(lex, tk))
      return false;

    /* Add the new token to the list of tokens. */
    lex->peeked[lex->num_peeked++] = *tk;
    return true;
  }

  /* Return the first peeked token */
  *tk = lex->peeked[0];
  return true;
}

bool lexer_peek_n(struct lexer *lex, size_t n, struct token *tk) {
  while (lex->num_peeked < n) {
    if (!lexer_next_no_peek(lex, tk))
      return false;


    lex->peeked[lex->num_peeked++] = *tk;
  }

  *tk = lex->peeked[n - 1];
  return true;
}
