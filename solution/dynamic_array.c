#include "dynamic_array.h"

#include <stdlib.h>

/**
 * TODO: Implement all the methods with declarations in dynamic_array.h here,
 * before proceeding with your project
 */

DynamicArray *da_create(size_t init_capacity) {
  DynamicArray *da = malloc(sizeof(DynamicArray));
  da->data = malloc(sizeof(char *) * init_capacity);
  da->size = 0;
  da->capacity = init_capacity;

  return da;
}

void da_put(DynamicArray *da, const char *val) {
  if (da->size == da->capacity) {
    da->capacity *= 2;
    realloc(da->data, da->capacity * sizeof(char *));
  }

  da->data[da->size++] = val;
}