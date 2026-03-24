#include <stdlib.h>
#include <string.h>
#include "../include/file.h" // Assurez-vous que le chemin correspond à votre arborescence

// Fonction d'initialisation (Constructeur)
file_t* initFile(char filename[MAX_FILENAME], int length, MD5 key, int piece_size) {
    // 1. Allocation dynamique de la mémoire pour la structure
    file_t* new_file = (file_t*)malloc(sizeof(file_t));
    // 2. Copie sécurisée du nom de fichier
    // On copie au maximum MAX_FILENAME - 1 caractères pour garder de la place pour le '\0'
    strncpy(new_file->filename, filename, MAX_FILENAME - 1);
    new_file->filename[MAX_FILENAME - 1] = '\0'; // Garantie que la chaîne se termine bien

    // 3. Assignation de la taille totale du fichier
    new_file->length = length;

    strncpy(new_file->key, key, 33);
    new_file->key[32] = '\0'; 
    new_file->piece_size = piece_size;

    return new_file;
}

void freeFile(file_t* file) {
    // Toujours vérifier que le pointeur n'est pas NULL avant de faire un free() !
    if (file != NULL) {
        free(file);
    }
}