#ifndef TRACKER_H
#define TRACKER_H

#include "peer.h"
#include "file.h"

#define MAX_PEERS 5 


typedef struct tracker_t {
    peer_t* peers[MAX_PEERS];
} tracker_t;

tracker_t* initTracker();
void freeTracker(tracker_t*);

peer_t* addPeer(tracker_t*, char ipAddr[ADDRESS_LEN], int port); //for the announce request
int removePeer(tracker_t*, peer_t*); 

//parsers
int handleAnnounce(tracker_t*, peer_t*, char** saveptr, char* response_buffer);
int handleLook(tracker_t*, char** saveptr, char* response_buffer);
int handleGetfile(tracker_t*, char** saveptr, char* response_buffer);
int handleUpdate(tracker_t*, peer_t*, char** saveptr, char* response_buffer);


#endif // TRACKER_H