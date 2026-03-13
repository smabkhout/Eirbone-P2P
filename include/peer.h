#ifndef PEER_H
#define PEER_H


#define HASH_LEN 16
#define NUM_HASHES 100

struct peer_t {
    int id;
    int state;
    char[16] ipAddr;
    int listeningPort;
    char[HASH_LEN] myFiles; 
}

void initPeer();
int requestFile(int fileID);
int updateStatus();






#endif // PEER_H