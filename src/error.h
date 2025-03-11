#ifndef ERROR_H
#define ERROR_H

#include <stddef.h>
#include <stdio.h>

/* This is fucking slow, use it sparingly */
size_t get_linenum(const char *src, const char *loc);

/* This is fucking slow, use it sparingly */
size_t get_linecol(const char *src, const char *loc);

void fprint_source_view(FILE *fptr, const char *src, size_t from, size_t to);
void fprint_error_ctx(FILE *fptr, const char *src, size_t padding,
  int offset, size_t len, const char *loc, const char *fmt, ...);
void fprint_info_ctx(FILE *fptr, const char *src, size_t padding,
  int offset, size_t len, const char *loc, const char *fmt, ...);

void fprint_note(FILE *fptr, const char *fmt, ...);
void fprint_help(FILE *fptr, const char *fmt, ...);
void fprint_error(FILE *fptr, const char *fmt, ...);

#endif /* ERROR_H */
