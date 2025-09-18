#include "dynamic_array.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/**
 * TODO: Implement all the methods with declarations in dynamic_array.h here,
 * before proceeding with your project
 */

// Create a new DynamicArray with given initial capacity
DynamicArray *da_create(size_t init_capacity) {
  DynamicArray *da = malloc(sizeof(DynamicArray));

  if (da == NULL) {
    fprintf(stderr, "malloc() failed trying to allocate da.\n");
    da_free(da);
    exit(1);
  }

  da->data = malloc(sizeof(char *) * init_capacity);
  if (da->data == NULL) {
    fprintf(stderr, "malloc() failed trying to allocate da->data.\n");
    da_free(da);
    exit(1);
  }

  da->size = 0;
  da->capacity = init_capacity;

  return da;
}

// Add element to Dynamic Array at the end. Handles resizing if necessary
void da_put(DynamicArray *da, char *val) {
  if (da->size == da->capacity) {
    da->capacity *= 2;
    da->data = realloc(da->data, da->capacity * sizeof(char *));
    if (da->data == NULL) {
      fprintf(stderr, "realloc() failed trying to double da capacity.");
      da_free(da);
      exit(1);
    }
  }

  da->data[da->size++] = val;
}

// Get element at an index (NULL if not found)
char *da_get(DynamicArray *da, const size_t ind) {
  if (ind >= da->size) {
    da_free(da);
    return NULL;
  }

  return da->data[ind];
}

// Delete Element at an index (handles packing)
void da_delete(DynamicArray *da, const size_t ind) {
  if (ind >= da->size) {
    da_free(da);
    return;
  }

  if (ind < da->size - 1) {
    memmove(&da->data[ind], &da->data[ind + 1],
            (da->size - ind - 1) * sizeof(char *));
  }

  da->size--;
}

// Print Elements line after line
void da_print(DynamicArray *da) {
  for (size_t i = 0; i < da->size; i++) {
    printf("%s\n", da->data[i]);
  }
}

// Free whole DynamicArray
void da_free(DynamicArray *da) {
  free(da->data);
  free(da);
}