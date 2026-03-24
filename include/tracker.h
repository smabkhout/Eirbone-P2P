#ifndef TRACKER_H
#define TRACKER_H

#include "peer.h"
#include "file.h"

#define MAX_PEERS 5 


typedef struct tracker_t {
    peer_t* peers[MAX_PEERS];
} tracker_t;

peer_t* addPeer(tracker_t*, char ipAddr[16], int port); //for the announce request
void removePeer(tracker_t*, peer_t*); 
void updatePeer(tracker_t*, peer_t*, MD5* seeded_files, MD5* leeched_files); //for the update request

//will be useful for update probably (maybe for anounce?)
void linkPeerToFile(tracker_t*, peer_t*, file_t*, enum fileType); //for the update request (and maybe anounce?)

// getfile request
file_t* findFileByKey(tracker_t* , MD5 key); //maybe just a helper for the one below? (static in .c!?)
peer_t** getPeersForFile(tracker_t*, file_t* file);

// look request
file_t** findFilesByCriteria(tracker_t*, char* criteria); 
int handle_announce(tracker_t* tracker, peer_t* current_peer, char** saveptr, char* response_buffer);



#endif // TRACKER_H