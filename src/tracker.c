#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../include/tracker.h"


peer_t* addPeer(tracker_t* tracker, char ipAddr[16], int port)
{
    for (int i = 0; i < MAX_PEERS; i++){
        if (tracker->peers[i] == NULL){
            tracker->peers[i] = initPeer(ipAddr, port);
            return tracker->peers[i];
        }
    }
    return NULL;
}

void removePeer(tracker_t* tracker, peer_t* peer_to_remove)
{
    if (peer_to_remove == NULL) return;
    for (int i = 0; i < MAX_PEERS; i++){
        if (tracker->peers[i] == peer_to_remove){
            freePeer(tracker->peers[i]);
            tracker->peers[i] = NULL;
            break;
        }
    }
}

file_t* addFile(tracker_t* tracker, char filename[MAX_FILENAME], int length, int piece_size, MD5 key)
{
    file_t* existing = findFileByKey(tracker, key);
    if (existing != NULL) return existing;

    for (int i = 0; i < MAX_PEERS; i++){
        if (tracker->files[i] == NULL){
            tracker->files[i] = initFile(filename, length, key);
            tracker->files[i]->piece_size = piece_size;
            return tracker->files[i];
        }
    }
    return NULL;
}

void linkPeerToFile(tracker_t* tracker, peer_t* peer, file_t* file, enum fileType type) {
    if (tracker == NULL || peer == NULL || file == NULL) return;

    if (type == SEEDER) {
        // 1. On vérifie s'il ne seed pas déjà ce fichier (pour éviter les doublons)
        for (int i = 0; i < MAX_FILES; i++) {
            if (strcmp(peer->seededFiles[i], file->key) == 0) {
                return; // Il l'a déjà, on s'arrête
            }
        }

        // 2. On l'ajoute dans le premier emplacement vide de ses seeders
        for (int i = 0; i < MAX_FILES; i++) {
            if (peer->seededFiles[i][0] == '\0') { // '\0' indique une case vide
                strncpy(peer->seededFiles[i], file->key, 33);
                break;
            }
        }

        // 3. LOGIQUE MÉTIER : S'il était leecher de ce fichier, il ne l'est plus !
        // (Utile pour la requête "update" quand un téléchargement se termine)
        for (int i = 0; i < MAX_FILES; i++) {
            if (strcmp(peer->leechedFiles[i], file->key) == 0) {
                peer->leechedFiles[i][0] = '\0'; // On vide la case
                break;
            }
        }
    } 
 
    else if (type == LEECHER) {
        // 1. On vérifie s'il ne l'a pas déjà (en leech ou en seed)
        for (int i = 0; i < MAX_FILES; i++) {
            if (strcmp(peer->leechedFiles[i], file->key) == 0 || 
                strcmp(peer->seededFiles[i], file->key) == 0) {
                return; 
            }
        }

        // 2. On l'ajoute dans le premier emplacement vide de ses leechers
        for (int i = 0; i < MAX_FILES; i++) {
            if (peer->leechedFiles[i][0] == '\0') {
                strncpy(peer->leechedFiles[i], file->key, 33);
                break;
            }
        }
    }
}

// 1. Handler pour la commande "announce"
int handle_announce(tracker_t* tracker, peer_t* current_peer, char** saveptr, char* response_buffer) {
    char* listen_kw = strtok_r(NULL, " ", saveptr);
    char* port_str = strtok_r(NULL, " ", saveptr);
    
    if (!listen_kw || !port_str || strcmp(listen_kw, "listen") != 0) return -1;

    current_peer->listeningPort = atoi(port_str);
    
    // Traitement de la liste "seed"
    char* seed_kw = strtok_r(NULL, " [", saveptr);
    if (seed_kw && strcmp(seed_kw, "seed") == 0) {
        char* file_name;
        while ((file_name = strtok_r(NULL, " ]", saveptr)) != NULL) {
            if (strcmp(file_name, "leech") == 0) break; // Fin des seeds, passage aux leechs
            
            char* length_str = strtok_r(NULL, " ]", saveptr);
            char* piece_str = strtok_r(NULL, " ]", saveptr);
            char* key_str = strtok_r(NULL, " ]", saveptr);
            
            if (length_str && piece_str && key_str) {
                file_t* f = addFile(tracker, file_name, atoi(length_str), atoi(piece_str), key_str);
                linkPeerToFile(tracker, current_peer, f, SEEDER);
            } else {
                return -1; // Il manque des informations pour ce fichier
            }
        }
    }
    
    // (Le traitement des "leech" viendrait ici de la même manière)

    sprintf(response_buffer, "ok\n");
    return 0; // Succès
}

