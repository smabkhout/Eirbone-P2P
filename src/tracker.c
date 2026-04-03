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

int removePeer(tracker_t* tracker, peer_t* peer_to_remove)
{
    if (peer_to_remove == NULL) return 0;
    for (int i = 0; i < MAX_PEERS; i++){
        if (tracker->peers[i] == peer_to_remove){
            freePeer(tracker->peers[i]);
            tracker->peers[i] = NULL;
            return 0;
        }
    }
    return -1;
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
    char* seed_kw = strtok_r(NULL, " ", saveptr);

    if (seed_kw && strcmp(seed_kw, "seed") == 0) {

        while ((file_name = strtok_r(NULL, " []]", saveptr)) != NULL) {
            if (strcmp(file_name, "leech") == 0) break; 
            
            char* length_str = strtok_r(NULL, " []\r\n", saveptr);
            char* piece_str  = strtok_r(NULL, " []\r\n", saveptr);
            char* key_str    = strtok_r(NULL, " []\r\n", saveptr);
            
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

void parse_look_criteria(char** saveptr, char* target_filename, int* target_filesize) {
    char* criterion;
    while ((criterion = strtok_r(NULL, " []\r\n", saveptr)) != NULL) {
        if (strncmp(criterion, "filename=", 9) == 0) {
            strncpy(target_filename, criterion + 9, MAX_FILENAME - 1);
        } else if (strncmp(criterion, "filesize>", 9) == 0) {
            *target_filesize = atoi(criterion + 9); // On modifie la valeur pointée
        }
    }
}

// Helper 2 : Vérifie si un fichier est déjà dans notre liste de résultats
int is_file_duplicate(file_t** found_files, int found_count, char* key) {
    for (int k = 0; k < found_count; k++) {
        if (strcmp(found_files[k]->key, key) == 0) {
            return 1; // C'est un doublon
        }
    }
    return 0; // Ce n'est pas un doublon
}

// Helper 3 : Parcourt tout le réseau pour trouver les fichiers qui matchent
void search_files_in_network(tracker_t* tracker, char* target_filename, int target_filesize, file_t** found_files, int* found_count) {
    for (int i = 0; i < MAX_PEERS; i++) {
        peer_t* current_peer = tracker->peers[i];
        if (current_peer == NULL) continue;

        file_t** list_to_check[2] = { current_peer->seededFiles, current_peer->leechedFiles };
        
        for (int list_idx = 0; list_idx < 2; list_idx++) {
            file_t** current_list = list_to_check[list_idx];

            for (int j = 0; j < MAX_FILES; j++) {
                file_t* f = current_list[j];
                if (f == NULL) continue;

                // Test des critères
                int match = 1;
                if (target_filename[0] != '\0' && strcmp(f->filename, target_filename) != 0) match = 0;
                if (target_filesize != -1 && f->length <= target_filesize) match = 0;

                // Ajout si ça matche et que ce n'est pas un doublon
                if (match && !is_file_duplicate(found_files, *found_count, f->key)) {
                    if (*found_count < 100) {
                        found_files[*found_count] = f;
                        (*found_count)++; // On incrémente le compteur
                    }
                }
            }
        }
    }
}

// Helper 4 : Transforme les objets fichiers trouvés en texte pour le réseau
void format_look_response(file_t** found_files, int found_count, char* response_buffer) {
    strcpy(response_buffer, "list [");
    
    for (int i = 0; i < found_count; i++) {
        char file_info[256];
        sprintf(file_info, "%s %d %d %s", 
                found_files[i]->filename, 
                found_files[i]->length, 
                found_files[i]->piece_size, 
                found_files[i]->key);
        
        strcat(response_buffer, file_info);
        if (i < found_count - 1) {
            strcat(response_buffer, " "); // Espace entre les fichiers
        }
    }
    
    strcat(response_buffer, "]\n");
}

int handle_look(tracker_t* tracker, char** saveptr, char* response_buffer) {
    // 1. Initialisation des variables de recherche
    char target_filename[MAX_FILENAME] = {0};
    int target_filesize = -1;
    
    // 2. Variables pour stocker les résultats
    file_t* found_files[100];
    int found_count = 0;

    
    
    // Etape 1 : Lire les critères
    parse_look_criteria(saveptr, target_filename, &target_filesize);

    // Etape 2 : Chercher dans le réseau
    search_files_in_network(tracker, target_filename, target_filesize, found_files, &found_count);

    // Etape 3 : Créer la réponse
    format_look_response(found_files, found_count, response_buffer);

    return 0; 
}


int handle_getfile(tracker_t* tracker, char** saveptr, char* response_buffer) {
    // 1. Extraction de la clé MD5 envoyée par le client
    char* key_str = strtok_r(NULL, " \r\n", saveptr);
    
    // Si la commande est mal formatée (il manque la clé)
    if (key_str == NULL) return -1; 

    // 2. Préparation du début de la réponse (ex: "peers 8905e92a... [")
    sprintf(response_buffer, "peers %s [", key_str);
    
    int first_peer_added = 1; // Un petit drapeau pour gérer proprement les espaces entre les IPs

    // 3. On parcourt tout le réseau (tous les pairs du Tracker)
    for (int i = 0; i < MAX_PEERS; i++) {
        peer_t* current_peer = tracker->peers[i];
        
        // Si la case du tableau contient bien un pair
        if (current_peer != NULL) {
            
            // On demande à notre fonction outil si ce pair a le fichier
            if (peerRequestFile(current_peer, key_str) != NONE) {
                
                // On ajoute un espace avant l'IP, SAUF si c'est le tout premier de la liste
                if (!first_peer_added) {
                    strcat(response_buffer, " ");
                }
                
                // On formate les infos du pair sous la forme "IP:Port" (ex: "1.1.1.2:2222")
                char peer_info[64];
                sprintf(peer_info, "%s:%d", current_peer->ipAddr, current_peer->listeningPort);
                
                // On colle ces infos dans le grand buffer de réponse
                strcat(response_buffer, peer_info);
                
                first_peer_added = 0; // Le premier pair est passé
            }
        }
    }

    // 4. On ferme la liste avec le crochet et on ajoute le retour à la ligne obligatoire
    strcat(response_buffer, "]\n");
    
    return 0; // Succès
}