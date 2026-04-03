#include <stdlib.h>
#include <string.h>
#include "../include/peer.h"


peer_t* initPeer(char* ipAddr, int listeningPort){
    struct peer_t* p = malloc(sizeof(struct peer_t));
    p->listeningPort = listeningPort;
    strncpy(p->ipAddr, ipAddr, ADDRESS_LEN-1);
    p->ipAddr[ADDRESS_LEN-1] = '\0';
    return p;
}

int peerAddSeed(peer_t* peer,   file_t* file){
    for (int i=0; i<MAX_FILES; i++){
        if (!peer->seededFiles[i]){
            peer->seededFiles[i] = file;
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

enum fileType peerRequestFile(peer_t* peer, MD5 fileKey){
    for (int i=0; i<MAX_FILES; i++){
        if (peer->leechedFiles[i]->key == fileKey){
            return LEECHER;
        }
        if (peer->seededFiles[i]->key == fileKey){
            return SEEDER;
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
    peer = NULL;
}