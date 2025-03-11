#include "arena.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#include <sys/mman.h>


struct region {
  struct region *next;
  size_t size;
  size_t capacity;
  uintptr_t data[];
};

struct arena {
  struct region *first, *last;
};

static struct region *region_new(size_t capacity) {
  size_t size_bytes = sizeof(struct region) + sizeof(uintptr_t) * capacity;

  struct region *region = mmap(NULL, size_bytes, PROT_READ | PROT_WRITE, MAP_ANONYMOUS | MAP_PRIVATE, -1, 0);

  region->next = NULL;
  region->capacity = capacity;
  region->size = 0;

  return region;
}

static void region_free(struct region *region) {
  size_t size_bytes = sizeof(struct region) + sizeof(uintptr_t) * region->capacity;
  munmap(region, size_bytes);
}


struct arena *arena_new() {
  struct arena *arena = malloc(sizeof(struct arena));

  arena->first = NULL;
  arena->last = NULL;

  return arena;
}

void *arena_alloc(struct arena *arena, size_t size) {
  size_t size_aligned = (size + sizeof(uintptr_t) - 1) / sizeof(uintptr_t);

  if (arena->last == NULL) {
    size_t capacity = 8 * 1024;
    if (capacity < size_aligned) capacity = size_aligned;

    arena->last = region_new(capacity);
    arena->first = arena->last;
  }

  while (arena->last->size + size_aligned > arena->last->capacity && arena->last->next != NULL) {
    arena->last = arena->last->next;
  }

  if (arena->last->size + size_aligned > arena->last->capacity) {
    size_t capacity = 8 * 1024;
    if (capacity < size_aligned) capacity = size_aligned;
    arena->last->next = region_new(capacity);
    arena->last = arena->last->next;
  }

  void *result = &arena->last->data[arena->last->size];
  arena->last->size += size_aligned;

  return result;
}

void arena_reset(struct arena *arena) {
  struct region *region = arena->first;
  
  for (; region != NULL; region = region->next)
    region->size = 0;

  arena->last = arena->first;
}

void arena_free(struct arena *arena) {
  struct region *region = arena->first;

  while (region != NULL) {
    struct region *old = region;
    region = region->next;

    region_free(old);
  }

  free(arena);
}

size_t arena_used(struct arena *arena) {
  struct region *region = arena->first;
  size_t total = 0;

  for (; region != NULL; region = region->next)
    total += region->size;

  total *= sizeof(uintptr_t);

  return total;
}

