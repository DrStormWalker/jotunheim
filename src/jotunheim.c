#include "sys/wait.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "error.h"
#include "lexer.h"
#include "parser.h"
#include "ast.h"
#include "arena.h"
#include "emit.h"

#ifndef JOTUNHEIM_VERSION
  #define JOTUNHEIM_VERSION "unversioned"
#endif

int exec_command(char *argv[]) {
  int status = 0;
  pid_t fk = fork();

  if (fk == 0) {
    execvp(argv[0], argv);
    exit(127);
  } else if (fk < 0) {
    return 1;
  } else if (fk > 0) {
    waitpid(fk, &status, 0);
  }

  return WEXITSTATUS(status);
}

int main(int argc, char *argv[]) {
  int status;
  FILE *fptr;
  char *src, *filename;
  size_t filesize, filename_len;

  printf("Jotunheim version: %s\n", JOTUNHEIM_VERSION);

  if (argc < 2) {
    fprintf(stderr, "Not enough arguments were supplied. Expected 1 got %d.\n", argc - 1);
    return 1;
  }

  filename = argv[1];
  fptr = fopen(filename, "r");

  if (fptr == NULL) {
    fprintf(stderr, "Failed to open file %s. %s\n", filename, strerror(errno));
    return 1;
  }

  fseek(fptr, 0L, SEEK_END);
  filesize = ftell(fptr);
  fseek(fptr, 0L, SEEK_SET);

  src = malloc(filesize + 1);
  fread(src, sizeof(char), filesize, fptr);
  src[filesize] = 0;

  fclose(fptr);

  struct lexer lex = lexer_new(src);
  struct arena *arena = arena_new();
  struct parser parser = parser_new(arena, &lex);
  struct ast ast;

  if (!parser_parse_ast(&parser, &ast)) {
    arena_free(arena);
    free(ast.consts);
    free(src);

    return 1;
  }

  struct string_buffer *buf = string_buffer_new();

  if (!emit_ast(buf, src, &ast)) {
    string_buffer_free(buf);
    arena_free(arena);
    free(ast.consts);
    free(src);

    return 1;
  }

  // printf("Ast used %zu bytes\n", arena_used(arena));

  FILE *ssa_fptr;
  // char ssa_filename[] = "/tmp/jotunheim-XXXXXX.ssa";
  char ssa_filename[] = "jotunheim.ssa";
  // ssa_fptr = fdopen(mkstemps(ssa_filename, 4), "w");
  ssa_fptr = fopen(ssa_filename, "w");

  string_buffer_dump_to_file(buf, ssa_fptr);

  fclose(ssa_fptr);

  string_buffer_free(buf);
  arena_free(arena);
  free(ast.consts);
  free(src);

  // char s_filename[] = "/tmp/jotunheim-XXXXXX.s";
  char s_filename[] = "jotunheim.s";
  close(mkstemps(s_filename, 2));

  status = exec_command((char *[]) {
    "qbe",
    "-o",
    s_filename,
    ssa_filename,
    NULL,
  });

  // unlink(ssa_filename);

  if (status != 0) {
    unlink(s_filename);
    return 1;
  }

  char *out_filename, *last_slash, *last_dot;

  last_slash = strrchr(filename, '/');
  last_dot = strrchr(filename, '.');

  filename_len = last_dot > last_slash ? last_dot - filename : strlen(filename);

  out_filename = malloc(filename_len + 1);
  strcpy(out_filename, filename);
  out_filename[filename_len] = 0;

  status = exec_command((char *[]) {
    "cc",
    "-Wno-unused-command-line-argument",
    "-o",
    out_filename,
    s_filename,
    NULL,
  });

  // unlink(s_filename);
  free(out_filename);

  if (status != 0)
    return 1;

  return 0;
}
