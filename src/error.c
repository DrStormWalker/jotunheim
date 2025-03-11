#include "error.h"

#include <stdio.h>
#include <stdarg.h>

#define BRED "\e[1;31m"
#define BBLU "\e[1;34m"
#define BWHT "\e[1;37m"
#define CRESET "\e[0m"

/* returns the number of the line `loc` is in.
 * if there is a null character between `src` and `loc`,
 * returns the number of lines - 1 in `src`.
 */
size_t get_linenum(const char *src, const char *loc) {
  size_t linenum = 0;

  /* Count the number of lines before `loc` */
  while (*src != 0 && src < loc) {
    if (*src == '\n')
      linenum++;

    src++;
  }

  return linenum;
}

/* returns the column number of `loc` is in.
 * if there is a null character between `src` and `loc`,
 * returns the number of characters in the last line of
 * `src` - 1.
 */
size_t get_linecol(const char *src, const char *loc) {
  const char *line_start = src;

  /* Skip to the line before `loc` */
  while (*src != 0 && src < loc) {
    if (*src == '\n')
      line_start = src + 1;

    src++;
  }

  return loc - line_start;
}

void fprint_source_view(FILE *fptr, const char *src, size_t from, size_t to) {
  /* Get the length of the largest line number. */
  size_t last_line = to - 1;
  size_t longest_linenum_len = snprintf(NULL, 0, "%ld", last_line);

  size_t last_linenum = 0;
  const char *line_start = src, *line_end = src;

  for (size_t linenum = from; linenum < to; linenum++) {
    while (last_linenum < linenum + 1) {
      if (*src == '\n') {
        line_start = line_end;
        line_end = src + 1;
        last_linenum++;
      }

      src++;
    }

    fprintf(fptr, " %*ld \e[1;34m|\e[0m %.*s\n", (int)longest_linenum_len, linenum + 1, (int)(line_end - line_start - 1), line_start);
  }
}

void fprint_error_ctx(FILE *fptr, const char *src, size_t padding,
  int offset, size_t len, const char *loc, const char *fmt, ...)
{
  va_list vargs;
  size_t linenum = get_linenum(src, loc);

  /* `padding` lines above line */
  size_t longest_linenum_len = snprintf(NULL, 0, "%ld", linenum + padding);

  size_t last_linenum = 0;
  const char *line_start = src, *line_end = src;

  for (size_t l = linenum > padding ? linenum - padding : 0; l < linenum; l++) {
    while (last_linenum < l + 1) {
      if (*src == '\n') {
        line_start = line_end;
        line_end = src + 1;
        last_linenum++;
      }

      src++;
    }

    fprintf(fptr, " %*ld " BBLU "|" CRESET " %.*s\n", (int)longest_linenum_len, l + 1, (int)(line_end - line_start - 1), line_start);
  }

  while (last_linenum < linenum + 1) {
    if (*src == '\n') {
      line_start = line_end;
      line_end = src + 1;
      last_linenum++;
    }

    src++;
  }

  size_t colnum = loc - line_start;

  fprintf(fptr, " %*ld " BBLU "|" CRESET " %.*s\n", (int)longest_linenum_len, linenum + 1, (int)(line_end - line_start - 1), line_start);
  fprintf(fptr, " %*s " BBLU "|" CRESET " %*s" BRED, (int)longest_linenum_len, "", (int)(colnum + offset), "");

  for (size_t i = 0; i < len; i++)
    fputc('~', fptr);
  fputc(' ', fptr);

  va_start(vargs, fmt);
  vfprintf(fptr, fmt, vargs);
  va_end(vargs);

  fprintf(fptr, CRESET "\n");
  
  for (size_t l = linenum + 1; l < linenum + padding + 1; l++) {
    while (last_linenum < l + 1) {
      if (*src == '\n') {
        line_start = line_end;
        line_end = src + 1;
        last_linenum++;
      }

      src++;
    }

    fprintf(fptr, " %*ld " BBLU "|" CRESET " %.*s\n", (int)longest_linenum_len, l + 1, (int)(line_end - line_start - 1), line_start);
  }
}

void fprint_info_ctx(FILE *fptr, const char *src, size_t padding,
  int offset, size_t len, const char *loc, const char *fmt, ...)
{
  va_list vargs;
  size_t linenum = get_linenum(src, loc);

  /* `padding` lines above line */
  size_t longest_linenum_len = snprintf(NULL, 0, "%ld", linenum + padding);

  size_t last_linenum = 0;
  const char *line_start = src, *line_end = src;

  for (size_t l = linenum > padding ? linenum - padding : 0; l < linenum; l++) {
    while (last_linenum < l + 1) {
      if (*src == '\n') {
        line_start = line_end;
        line_end = src + 1;
        last_linenum++;
      }

      src++;
    }

    fprintf(fptr, " %*ld " BBLU "|" CRESET " %.*s\n", (int)longest_linenum_len, l + 1, (int)(line_end - line_start - 1), line_start);
  }

  while (last_linenum < linenum + 1) {
    if (*src == '\n') {
      line_start = line_end;
      line_end = src + 1;
      last_linenum++;
    }

    src++;
  }

  size_t colnum = loc - line_start;

  fprintf(fptr, " %*ld " BBLU "|" CRESET " %.*s\n", (int)longest_linenum_len, linenum + 1, (int)(line_end - line_start - 1), line_start);
  fprintf(fptr, " %*s " BBLU "|" CRESET " %*s" BBLU, (int)longest_linenum_len, "", (int)(colnum + offset), "");

  for (size_t i = 0; i < len; i++)
    fputc('-', fptr);
  fputc(' ', fptr);

  va_start(vargs, fmt);
  vfprintf(fptr, fmt, vargs);
  va_end(vargs);

  fprintf(fptr, CRESET "\n");
  
  for (size_t l = linenum + 1; l < linenum + padding + 1; l++) {
    while (last_linenum < l + 1) {
      if (*src == 0) {
        return;
      }

      if (*src == '\n') {
        line_start = line_end;
        line_end = src + 1;
        last_linenum++;
      }

      src++;
    }

    fprintf(fptr, " %*ld " BBLU "|" CRESET " %.*s\n", (int)longest_linenum_len, l + 1, (int)(line_end - line_start - 1), line_start);
  }
}

void fprint_note(FILE *fptr, const char *fmt, ...) {
  va_list vargs;

  fprintf(fptr, BBLU "note" CRESET ": ");

  va_start(vargs, fmt);
  vfprintf(fptr, fmt, vargs);
  va_end(vargs);

  fputc('\n', fptr);
}

void fprint_help(FILE *fptr, const char *fmt, ...) {
  va_list vargs;

  fprintf(fptr, BBLU "help" CRESET ": ");

  va_start(vargs, fmt);
  vfprintf(fptr, fmt, vargs);
  va_end(vargs);

  fputc('\n', fptr);
}

void fprint_error(FILE *fptr, const char *fmt, ...) {
  va_list vargs;

  fprintf(fptr, BRED "error" CRESET ": ");

  va_start(vargs, fmt);
  vfprintf(fptr, fmt, vargs);
  va_end(vargs);

  fputc('\n', fptr);
}
