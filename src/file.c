#include <stdlib.h>
#include <string.h>
#include "../include/file.h"

file_t* initFile(char filename[MAX_FILENAME], int length, MD5 key, int piece_size) {
    file_t* new_file = (file_t*)malloc(sizeof(file_t));
    strncpy(new_file->filename, filename, MAX_FILENAME - 1);
    new_file->filename[MAX_FILENAME - 1] = '\0';
    new_file->length = length;

    strncpy(new_file->key, key, 33);
    new_file->key[32] = '\0'; 
    new_file->piece_size = piece_size;

    return new_file;
}

void freeFile(file_t* file) {
    if (file != NULL) { free(file); }
}