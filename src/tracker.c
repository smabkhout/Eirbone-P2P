#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../include/tracker.h"


tracker_t* initTracker(){
    tracker_t* tracker = malloc(sizeof(tracker_t));
    if (tracker == NULL) {
        fprintf(stderr, "Erreur d'allocation pour le tracker\n");
        return NULL;
    }
    for (int i = 0; i < MAX_PEERS; i++){
        tracker->peers[i] = NULL;
    }
    return tracker;
}

void freeTracker(tracker_t* tracker){
    if (tracker == NULL) return;
    for (int i = 0; i < MAX_PEERS; i++){
        if (tracker->peers[i] != NULL){
            freePeer(tracker->peers[i]);
            tracker->peers[i] = NULL;
        }
    }
    free(tracker);
}


peer_t* addPeer(tracker_t* tracker, const char* ipAddr, int port)
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

static file_t* findFileByKey(tracker_t* tracker, MD5 key) {
    if (tracker == NULL || key == NULL) return NULL;
    for (int i = 0; i < MAX_PEERS; i++) {
        peer_t* current_peer = tracker->peers[i];        
        if (current_peer != NULL) {   
            file_t** lists[2] = { current_peer->seededFiles, current_peer->leechedFiles };         
            for (int l = 0; l < 2; l++){
                for (int j = 0; j < MAX_FILES; j++){
                    if (lists[l][j] != NULL && strcmp(lists[l][j]->key, key) == 0)
                        return lists[l][j];
                }
            }
        }
    }
    return NULL; 
}

static int parseSeedList(tracker_t* tracker, peer_t* peer, char **filename, char **saveptr, char* response_buffer){

    char *tmp_filename;
    while ((tmp_filename = strtok_r(NULL, " []]", saveptr)) != NULL) {
        if (strcmp(tmp_filename, "leech") == 0) {
            *filename = tmp_filename;
            break; 
        }
            
        char* length_str = strtok_r(NULL, " []\r\n", saveptr);
        char* piece_str  = strtok_r(NULL, " []\r\n", saveptr);
        char* key_str    = strtok_r(NULL, " []\r\n", saveptr);
            
        if (length_str && piece_str && key_str) {
            if (!findFileByKey(tracker, key_str)) {
                file_t* f = initFile(tmp_filename, atoi(length_str), key_str, atoi(piece_str));
                peerAddSeed(peer, f);
            } else {
                printf("Log : Fichier %s déjà connu ignoré en seed.\n", key_str);
            }                
        } else {
            sprintf(response_buffer, "KO: Il manque des informations pour ce fichier\n");
            return -1;
        }
    }
    return 0;
}

static int parseLeechList(tracker_t* tracker, peer_t* peer, char **saveptr){
    char *key_str;
    while ((key_str = strtok_r(NULL, " []\r\n", saveptr)) != NULL) {
        file_t* existing_file = findFileByKey(tracker, key_str);
        if (existing_file != NULL) {
            peerAddLeech(peer, existing_file);
        } else {
            printf("Log: Fichier %s inconnu ignoré en leech.\n", key_str);
        }
    }
    return 0;
}

int handleAnnounce(tracker_t* tracker, peer_t* current_peer, char** saveptr, char* response_buffer) {
    char* listen_kw = strtok_r(NULL, " ", saveptr);
    char* port_str = strtok_r(NULL, " ", saveptr);
    
    if (!listen_kw || !port_str || strcmp(listen_kw, "listen") != 0) return -1;

    current_peer->listeningPort = atoi(port_str);
    char* file_name = NULL;
    char* seed_kw = strtok_r(NULL, " ", saveptr);
    if (seed_kw && strcmp(seed_kw, "seed") == 0) {
        if (parseSeedList(tracker, current_peer, &file_name, saveptr, response_buffer) != 0) {
            return -1; 
        }
    }
    
    char* leech_kw = NULL;
    if (file_name && strcmp(file_name, "leech") == 0) {
        leech_kw = file_name;
    } else {
        leech_kw = strtok_r(NULL, " [", saveptr);
    }
    if (leech_kw != NULL && strcmp(leech_kw, "leech") == 0) {
        if (parseLeechList(tracker, current_peer, saveptr) != 0) {
            return -1; 
        }   
    }
    sprintf(response_buffer, "ok\n");
    return 0;
}

static void parseLookCriteria(char** saveptr, char* target_filename, int* target_filesize) {
    char* criterion;
    while ((criterion = strtok_r(NULL, " []\r\n", saveptr)) != NULL) {
        if (strncmp(criterion, "filename=", 9) == 0) {
            strncpy(target_filename, criterion + 9, MAX_FILENAME - 1);
        } else if (strncmp(criterion, "filesize>", 9) == 0) {
            *target_filesize = atoi(criterion + 9);
        }
    }
}

static int isFileDuplicate(file_t** found_files, int found_count, char* key) {
    for (int k = 0; k < found_count; k++) {
        if (strcmp(found_files[k]->key, key) == 0) {
            return 1;
        }
    }
    return 0; 
}

static int fileMatchesCriteria(file_t* f, char* target_filename, int target_filesize) {
    if (target_filename[0] != '\0' && strcmp(f->filename, target_filename) != 0) 
        return 0;
    if (target_filesize != -1 && f->length <= target_filesize) 
        return 0;
    return 1;
}

static void searchFilesInNetwork(tracker_t* tracker, char* target_filename, int target_filesize, file_t** found_files, int* found_count) {
    for (int i = 0; i < MAX_PEERS; i++) {
        peer_t* current_peer = tracker->peers[i];
        if (current_peer == NULL) continue;

        file_t** list_to_check[2] = { current_peer->seededFiles, current_peer->leechedFiles };
        
        for (int list_idx = 0; list_idx < 2; list_idx++) {
            file_t** current_list = list_to_check[list_idx];

            for (int j = 0; j < MAX_FILES; j++) {
                file_t* f = current_list[j];
                if (f == NULL) continue;
                if (fileMatchesCriteria(f, target_filename, target_filesize) && 
                        !isFileDuplicate(found_files, *found_count, f->key)) {
                    if (*found_count < 100) {
                        found_files[*found_count] = f;
                        (*found_count)++;
                    }
                }
            }
        }
    }
}

static void formatLookResponse(file_t** found_files, int found_count, char* response_buffer) {
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

int handleLook(tracker_t* tracker, char** saveptr, char* response_buffer) {
    char target_filename[MAX_FILENAME] = {0};
    int target_filesize = -1;
    
    file_t* found_files[100];
    int found_count = 0;

    parseLookCriteria(saveptr, target_filename, &target_filesize);
    searchFilesInNetwork(tracker, target_filename, target_filesize, found_files, &found_count);
    formatLookResponse(found_files, found_count, response_buffer);

    return 0; 
}


int handleGetfile(tracker_t* tracker, char** saveptr, char* response_buffer) {
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