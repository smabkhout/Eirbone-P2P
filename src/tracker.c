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

file_t* findFileByKey(tracker_t* tracker, MD5 key) {
    if (tracker == NULL || key == NULL) return NULL;

    // 1. On parcourt tous les pairs actuellement connectés au Tracker
    for (int i = 0; i < MAX_PEERS; i++) {
        peer_t* current_peer = tracker->peers[i];
        
        if (current_peer != NULL) {
            
            // 2. On cherche d'abord dans les fichiers qu'il possède entièrement (seeds)
            for (int j = 0; j < MAX_FILES; j++) {
                file_t* f = current_peer->seededFiles[j];
                // Si la case n'est pas vide et que la clé correspond
                if (f != NULL && strcmp(f->key, key) == 0) {
                    return f; // On a trouvé le fichier ! On le renvoie.
                }
            }
            
            // 3. On cherche ensuite dans les fichiers qu'il télécharge (leechs)
            for (int j = 0; j < MAX_FILES; j++) {
                file_t* f = current_peer->leechedFiles[j];
                // Si la case n'est pas vide et que la clé correspond
                if (f != NULL && strcmp(f->key, key) == 0) {
                    return f; // On a trouvé le fichier ! On le renvoie.
                }
            }
        }
    }

    // Si on a fait le tour de tout le monde et qu'on n'a rien trouvé
    return NULL; 
}

// 1. Handler pour la commande "announce"
int handle_announce(tracker_t* tracker, peer_t* current_peer, char** saveptr, char* response_buffer) {
    char* listen_kw = strtok_r(NULL, " ", saveptr);
    char* port_str = strtok_r(NULL, " ", saveptr);
    
    if (!listen_kw || !port_str || strcmp(listen_kw, "listen") != 0) return -1;

    current_peer->listeningPort = atoi(port_str);
    char* file_name;
    // Traitement de la liste "seed"
    char* seed_kw = strtok_r(NULL, " [", saveptr);
    if (seed_kw && strcmp(seed_kw, "seed") == 0) {

        while ((file_name = strtok_r(NULL, " ]", saveptr)) != NULL) {
            if (strcmp(file_name, "leech") == 0) break; // Fin des seeds, passage aux leechs
            
            char* length_str = strtok_r(NULL, " ]", saveptr);
            char* piece_str = strtok_r(NULL, " ]", saveptr);
            char* key_str = strtok_r(NULL, " ]", saveptr);
            
            if (length_str && piece_str && key_str) {
                file_t* f = initFile(file_name, atoi(length_str), key_str, atoi(piece_str));
                peerAddSeed(current_peer, f);
            } else {
                sprintf(response_buffer, "KO: Il manque des informations pour ce fichier\n");
                return -1; // Il manque des informations pour ce fichier
            }
        }
    }
    
    char* leech_kw = NULL;
    
    // Si la boucle "seed" s'est arrêtée avec un "break" sur le mot "leech"
    if (file_name != NULL && strcmp(file_name, "leech") == 0) {
        leech_kw = file_name;
    } else {
        // Sinon (ex: si la liste seed était vide "seed [] leech [...]")
        leech_kw = strtok_r(NULL, " [", saveptr);
    }

    if (leech_kw != NULL && strcmp(leech_kw, "leech") == 0) {
        char* key_str;
        
        // On boucle pour lire uniquement les clés MD5
        while ((key_str = strtok_r(NULL, " ]\r\n", saveptr)) != NULL) {
            
            // 1. On cherche le fichier dans le réseau via sa clé
            file_t* existing_file = findFileByKey(tracker, key_str);
            
            // 2. Si on le trouve, on utilise votre fonction de peer.h
            if (existing_file != NULL) {
                peerAddLeech(current_peer, existing_file);
            } else {
                // Note : Si le fichier n'existe pas, on l'ignore silencieusement 
                // (On ne peut pas le créer car on n'a ni son nom ni sa taille)
                printf("Log : Fichier %s inconnu ignoré en leech.\n", key_str);
            }
        }
    }

    sprintf(response_buffer, "ok\n");
    return 0; // Succès
}

