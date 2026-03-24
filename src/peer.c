#include <stdlib.h>
#include "../include/peer.h"


peer_t* initPeer(char ipAddr[ADDRESS_LEN], int listeningPort){
    struct peer_t* p = malloc(sizeof(struct peer_t));
    p->listeningPort = listeningPort;
    for (int i=0; i<ADDRESS_LEN; i++){
        p->ipAddr[i] = ipAddr[i];
    }
    return p;
}

void peerAddSeed(peer_t* peer,   file_t* file){
    for (int i=0; i<MAX_FILES; i++){
        if (!peer->seededFiles[i]){
            peer->seededFiles[i] = malloc(sizeof(file_t));
            peer->seededFiles[i] = file;
        }
    }
}
void peerAddLeech(peer_t* peer, file_t* file){
    for (int i=0; i<MAX_FILES; i++){
        if (!peer->leechedFiles[i]){
            peer->leechedFiles[i] = malloc(sizeof(file_t));
            peer->leechedFiles[i] = file;
        }
    }
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

    for (int i=0; i<MAX_FILES; i++){
        if (peer->leechedFiles[i]){
            freeFile(peer->leechedFiles[i]);
        }
        if (peer->leechedFiles[i]){
            freeFile(peer->leechedFiles[i]);
        }
    }

    freePeer(peer);
}

