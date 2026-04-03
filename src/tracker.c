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

    for (int i = 0; i < MAX_PEERS; i++) {
        peer_t* current_peer = tracker->peers[i];
        
        if (current_peer != NULL) {
            
            for (int j = 0; j < MAX_FILES; j++) {
                file_t* f = current_peer->seededFiles[j];

                if (f != NULL && strcmp(f->key, key) == 0) {
                    return f; 
                }
            }
            
            for (int j = 0; j < MAX_FILES; j++) {
                file_t* f = current_peer->leechedFiles[j];

                if (f != NULL && strcmp(f->key, key) == 0) {
                    return f;
                }
            }
        }
    }

    return NULL; 
}

int handle_announce(tracker_t* tracker, peer_t* current_peer, char** saveptr, char* response_buffer) {
    char* listen_kw = strtok_r(NULL, " ", saveptr);
    char* port_str = strtok_r(NULL, " ", saveptr);
    
    if (!listen_kw || !port_str || strcmp(listen_kw, "listen") != 0) return -1;

    current_peer->listeningPort = atoi(port_str);
    char* file_name;

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
                return -1;
            }
        }
    }
    
    char* leech_kw = NULL;
    

    if (file_name != NULL && strcmp(file_name, "leech") == 0) {
        leech_kw = file_name;
    } else {

        leech_kw = strtok_r(NULL, " [", saveptr);
    }

    if (leech_kw != NULL && strcmp(leech_kw, "leech") == 0) {
        char* key_str;
        

        while ((key_str = strtok_r(NULL, " ]\r\n", saveptr)) != NULL) {
            

            file_t* existing_file = findFileByKey(tracker, key_str);
            

            if (existing_file != NULL) {
                peerAddLeech(current_peer, existing_file);
            } else {

                printf("Log : Fichier %s inconnu ignoré en leech.\n", key_str);
            }
        }
    }

    sprintf(response_buffer, "ok\n");
    return 0; // Succès
}

static void parse_look_criteria(char** saveptr, char* target_filename, int* target_filesize) {
    char* criterion;
    while ((criterion = strtok_r(NULL, " []\r\n", saveptr)) != NULL) {
        if (strncmp(criterion, "filename=", 9) == 0) {
            strncpy(target_filename, criterion + 9, MAX_FILENAME - 1);
        } else if (strncmp(criterion, "filesize>", 9) == 0) {
            *target_filesize = atoi(criterion + 9);
        }
    }
}

static int is_file_duplicate(file_t** found_files, int found_count, char* key) {
    for (int k = 0; k < found_count; k++) {
        if (strcmp(found_files[k]->key, key) == 0) {
            return 1;
        }
    }
    return 0; 
}

static void search_files_in_network(tracker_t* tracker, char* target_filename, int target_filesize, file_t** found_files, int* found_count) {
    for (int i = 0; i < MAX_PEERS; i++) {
        peer_t* current_peer = tracker->peers[i];
        if (current_peer == NULL) continue;

        file_t** list_to_check[2] = { current_peer->seededFiles, current_peer->leechedFiles };
        
        for (int list_idx = 0; list_idx < 2; list_idx++) {
            file_t** current_list = list_to_check[list_idx];

            for (int j = 0; j < MAX_FILES; j++) {
                file_t* f = current_list[j];
                if (f == NULL) continue;

                int match = 1;
                if (target_filename[0] != '\0' && strcmp(f->filename, target_filename) != 0) match = 0;
                if (target_filesize != -1 && f->length <= target_filesize) match = 0;

                if (match && !is_file_duplicate(found_files, *found_count, f->key)) {
                    if (*found_count < 100) {
                        found_files[*found_count] = f;
                        (*found_count)++;
                    }
                }
            }
        }
    }
}

static void format_look_response(file_t** found_files, int found_count, char* response_buffer) {
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
            strcat(response_buffer, " "); 
        }
    }
    
    strcat(response_buffer, "]\n");
}

int handle_look(tracker_t* tracker, char** saveptr, char* response_buffer) {
    char target_filename[MAX_FILENAME] = {0};
    int target_filesize = -1;
    
    file_t* found_files[100];
    int found_count = 0;

    parse_look_criteria(saveptr, target_filename, &target_filesize);
    search_files_in_network(tracker, target_filename, target_filesize, found_files, &found_count);
    format_look_response(found_files, found_count, response_buffer);

    return 0; 
}


int handle_getfile(tracker_t* tracker, char** saveptr, char* response_buffer) {
    char* key_str = strtok_r(NULL, " \r\n", saveptr);
    
    if (key_str == NULL) return -1; 

    sprintf(response_buffer, "peers %s [", key_str);
    
    int first_peer_added = 1;

    for (int i = 0; i < MAX_PEERS; i++) {
        peer_t* current_peer = tracker->peers[i];
        
        if (current_peer != NULL) {
            
            if (peerRequestFile(current_peer, key_str) != NONE) {
                
                if (!first_peer_added) {
                    strcat(response_buffer, " ");
                }
                
                char peer_info[64];
                sprintf(peer_info, "%s:%d", current_peer->ipAddr, current_peer->listeningPort);
                
                strcat(response_buffer, peer_info);
                
                first_peer_added = 0;
            }
        }
    }

    strcat(response_buffer, "]\n");
    
    return 0;
}