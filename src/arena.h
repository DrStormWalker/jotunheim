#ifndef _ALLOC_H_
#define _ALLOC_H_

#include <stddef.h>

struct arena;

struct arena *arena_new();

void *arena_alloc(struct arena *arena, size_t size);

void arena_reset(struct arena *arena);
void arena_free(struct arena *arena);

size_t arena_used(struct arena *arena);

#endif
