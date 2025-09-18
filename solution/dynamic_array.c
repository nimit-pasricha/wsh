#include "dynamic_array.h"

#include <stdlib.h>
#include <string.h>

/**
 * TODO: Implement all the methods with declarations in dynamic_array.h here,
 * before proceeding with your project
 */

// Create a new DynamicArray with given initial capacity
DynamicArray *da_create(size_t init_capacity) {
  DynamicArray *da = malloc(sizeof(DynamicArray));
  da->data = malloc(sizeof(char *) * init_capacity);
  da->size = 0;
  da->capacity = init_capacity;

  return da;
}

// Add element to Dynamic Array at the end. Handles resizing if necessary
void da_put(DynamicArray *da, const char *val) {
  if (da->size == da->capacity) {
    da->capacity *= 2;
    realloc(da->data, da->capacity * sizeof(char *));
  }

  da->data[da->size++] = val;
}

// Get element at an index (NULL if not found)
char *da_get(DynamicArray *da, const size_t ind) {
  if (ind < 0 || ind >= da->size) {
    return NULL;
  }

  return da->data[ind];
}

// Delete Element at an index (handles packing)
void da_delete(DynamicArray *da, const size_t ind) {
  if (ind < 0 || ind >= da->size) {
    return;
  }

  if (ind < da->size - 1) {
    memmove(&da->data[ind], &da->data[ind + 1],
            (da->size - ind - 1) * sizeof(char *));
  }

  da->size--;
}