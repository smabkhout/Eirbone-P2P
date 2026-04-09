#include <stdlib.h>
#include <string.h>
#include "../include/file.h"

file_t* initFile(char* filename, int length, MD5 key, int piece_size) {
    file_t* new_file = (file_t*)malloc(sizeof(file_t));
    if (new_file == NULL) return NULL;
    strncpy(new_file->filename, filename, MAX_FILENAME - 1);
    new_file->length = length;

    strncpy(new_file->key, key, MD5_LEN);
    new_file->piece_size = piece_size;

    return new_file;
}

void freeFile(file_t* file) {
    if (file != NULL) { free(file); }
}