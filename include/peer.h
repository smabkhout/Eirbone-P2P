#ifndef PEER_H
#define PEER_H

#include "file.h"


#define NUM_HASHES 100
#define MAX_FILES 10 
#define ADDRESS_LEN 16


typedef struct peer_t {
    char ipAddr[ADDRESS_LEN]; 
    int listeningPort;
    file_t* seededFiles[MAX_FILES];      
    file_t* leechedFiles[MAX_FILES];   
} peer_t;

//all helpers for the tracker
enum fileType{
    SEEDER,
    LEECHER,
    NONE
};

peer_t* initPeer(char ipAddr[ADDRESS_LEN], int listeningPort);

void peerAddSeed(peer_t*,   file_t* file); //used for anounce and update
void peerAddLeech(peer_t*, file_t* file);

enum fileType peerRequestFile(peer_t*, MD5 fileKey); //used by getPeersForFile

void freePeer(peer_t*);

#endif // PEER_H