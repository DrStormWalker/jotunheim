#include "vector.h"

#include <stdlib.h>
#include <string.h>

struct vector {
  size_t elsize;
  size_t cap;
  size_t len;
  void *items;
  void (*elfree)(void *item);
};

struct vector *vector_new(size_t elsize, size_t cap, 
  void (*elfree)(void *item))
{
  size_t ncap = 16;
  if (cap < ncap) {
    cap = ncap;
  }

  size_t size = sizeof(struct vector);
  struct vector *vec = malloc(size);

  vec->elsize = elsize;
  vec->cap = cap;
  vec->len = 0;
  vec->items = malloc(elsize * cap);
  vec->elfree = elfree;
  return vec;
}

inline static void *store_for(struct vector *vec, size_t i) {
  return vec->items + vec->elsize * i;
}

static void free_items(struct vector *vec) {
  if (vec->elfree) {
    for (size_t i = 0; i < vec->len; i++) {
      void *item = store_for(vec, i);
      vec->elfree(item);
    }
  }
}

void vector_free(struct vector *vec) {
  if (!vec) return;
  free_items(vec);
  free(vec->items);
  free(vec);
}


void vector_push(struct vector *vec, const void *item) {
  if (vec->len >= vec->cap) {
    vec->cap = vec->cap == 0 ? 16 : vec->cap * 2;
    vec->items = realloc(vec->items, vec->cap * vec->elsize);
  }

  memcpy(store_for(vec, vec->len++), item, vec->elsize);
}

bool vector_iter(struct vector *vec, size_t *i, void **item) {
  if (*i >= vec->len) return false;
  *item = store_for(vec, (*i)++);
  return true;
}

const void *vector_get(struct vector *vec, size_t i) {
  return store_for(vec, i);
}

size_t vector_into_inner(struct vector *vec, void **items) {
  size_t len = vec->len;
  *items = vec->items;
  free(vec);

  return len;
}
