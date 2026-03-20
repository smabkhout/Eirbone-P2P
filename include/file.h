#ifndef FILE_H
#define FILE_H

#define MAX_FILENAME 64
#define KEY_LEN 16

typedef char MD5[33]; //the hashes in the subject are 32 chars long + '\0' so 33?

typedef struct file_t {
    char filename[MAX_FILENAME];
    int length; //size in bytes
    MD5 key;
    int piece_size = 1024; //1KB pieces predefined in the subject
} file_t;

// all helpers for the tracker
file_t* initFile(char filename[MAX_FILENAME], int length, MD5 key);

void freeFile(file_t*);

#endif // FILE_H
