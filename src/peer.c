#include <stdlib.h>
#include <string.h>
#include "../include/peer.h"


peer_t* initPeer(char ipAddr[ADDRESS_LEN], int listeningPort){
    struct peer_t* p = malloc(sizeof(struct peer_t));
    p->listeningPort = listeningPort;
    strncpy(ipAddr, p->ipAddr, ADDRESS_LEN-1);
    p->ipAddr[ADDRESS_LEN-1] = '\0';
    return p;
}

void peerAddSeed(peer_t* peer,   file_t* file){
    for (int i=0; i<MAX_FILES; i++){
        if (!peer->seededFiles[i]){
            peer->seededFiles[i] = malloc(sizeof(file_t));
            peer->seededFiles[i] = file;
            return 0;
        }
    }
    return -1;
}
void peerAddLeech(peer_t* peer, file_t* file){
    for (int i=0; i<MAX_FILES; i++){
        if (!peer->leechedFiles[i]){
            peer->leechedFiles[i] = malloc(sizeof(file_t));
            peer->leechedFiles[i] = file;
            return 0;;
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

