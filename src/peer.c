#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#include "../include/peer.h"


peer_t* initPeer(char* ipAddr, int listeningPort){
    struct peer_t* p = malloc(sizeof(struct peer_t));
    p->listeningPort = listeningPort;
    strncpy(p->ipAddr, ipAddr, ADDRESS_LEN-1);
    p->ipAddr[ADDRESS_LEN-1] = '\0';
    for (int i = 0; i<MAX_FILES; i++){
        p->seededFiles[i] = NULL;
        p->leechedFiles[i] = NULL;
    }
    return p;
}

int peerAddSeed(peer_t* peer,   file_t* file){
    for (int i=0; i<MAX_FILES; i++){
        if (!peer->seededFiles[i]){
            peer->seededFiles[i] = file;
            printf("bonbon\n");
            return 0 ;
        }
    }
    return -1;
}

int peerAddLeech(peer_t* peer, file_t* file){
    for (int i=0; i<MAX_FILES; i++){
        if (!peer->leechedFiles[i]){
            peer->leechedFiles[i] = file;

            return 0;
        }
    }
    return -1;
}

enum fileType peerRequestFile(peer_t* peer, MD5 fileKey) {
    if (peer == NULL || fileKey == NULL) return NONE;

    for (int i = 0; i < MAX_FILES; i++) {
        
        if (peer->leechedFiles[i] != NULL) {
            if (strcmp(peer->leechedFiles[i]->key, fileKey) == 0) {
                return LEECHER;
            }
        }
        
        if (peer->seededFiles[i] != NULL) {
            if (strcmp(peer->seededFiles[i]->key, fileKey) == 0) {
                return SEEDER;
            }
        }
    }
    return NONE;
}

void freePeer(peer_t* peer){
    if (peer == NULL) return;
    for (int i=0; i<MAX_FILES; i++){
        if (peer->leechedFiles[i]){
            freeFile(peer->leechedFiles[i]);
        }
        if (peer->seededFiles[i]){
            freeFile(peer->seededFiles[i]);
        }
    }
    free(peer);
}