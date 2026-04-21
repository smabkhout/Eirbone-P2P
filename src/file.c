#include "../include/file.h"
#include <stdlib.h>
#include <string.h>

file_t *initFile(char *filename, int length, MD5 key, int piece_size) {
  file_t *new_file = (file_t *)malloc(sizeof(file_t));
  if (new_file == NULL)
    return NULL;
  strncpy(new_file->filename, filename, MAX_FILENAME - 1);
  new_file->filename[MAX_FILENAME - 1] = '\0';
  new_file->length = length;

  strncpy(new_file->key, key, MD5_LEN);
  new_file->key[MD5_LEN - 1] = '\0';
  new_file->piece_size = piece_size;
  new_file->ref_count = 0;

  return new_file;
}

void freeFile(file_t *file) {
  if (file != NULL) {
    free(file);
  }
}

void fileRetain(file_t *file) {
  if (file == NULL)
    return;
  file->ref_count++;
}

void fileRelease(file_t *file) {
  if (file == NULL)
    return;
  if (file->ref_count > 0) {
    file->ref_count--;
  }
  if (file->ref_count == 0) {
    freeFile(file);
  }
}