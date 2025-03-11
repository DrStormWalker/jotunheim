#ifndef _VECTOR_H_
#define _VECTOR_H_

#include <stddef.h>
#include <stdbool.h>

struct vector;

struct vector *vector_new(size_t elsize, size_t cap,
  void (*elfree)(void *item));

void vector_free(struct vector *vec);

void vector_push(struct vector *vec, const void *item);
const void *vector_get(struct vector *vec, size_t i);

bool vector_iter(struct vector *vec, size_t *i, void **item);

size_t vector_into_inner(struct vector *vec, void **items);

#endif
