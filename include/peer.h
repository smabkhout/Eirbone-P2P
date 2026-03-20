#ifndef PEER_H
#define PEER_H


#define NUM_HASHES 100
#define MAX_FILES 10 //10 for seeding and 10 for leeching?
#define ADDRESS_LEN 16

typedef char MD5[33];

typedef struct peer_t {
    char ipAddr[ADDRESS_LEN]; //salah darha char maert elah machi int!?
    int listeningPort;
    char seededFiles[MAX_FILES];    //store only file keys  
    char leechedFiles[MAX_FILES];   //store only file keys
} peer_t;

//all helpers for the tracker
enum fileType{
    SEEDER,
    LEECHER,
    NONE
};

peer_t* initPeer(char ipAddr[ADDRESS_LEN], int listeningPort);

void peerAddSeed(peer_t*, MD5 fileKey); //used for anounce and update
void peerAddLeech(peer_t*, MD5 fileKey);

enum fileType peerRequestFile(peer_t*, MD5 fileKey); //used by getPeersForFile

void freePeer(peer_t*);

#endif // PEER_H