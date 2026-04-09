#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include "../include/tracker.h"

void test_handleAnnounce() {
    printf("TEST handleAnnounce\n");

    tracker_t* my_tracker = initTracker();
    peer_t* my_peer = initPeer("Peer1", 0);
    char response[256];

    //all good here
    char good_raw_command[] = "announce listen 1111 seed [file_a.dat 2097152 1024 8905e92afeb80fc7722ec89eb0bf0966] leech []";
    char* saveptr;
    char* first_word = strtok_r(good_raw_command, " \r\n", &saveptr);

    assert(first_word && strcmp(first_word, "announce") == 0);
    int status = handleAnnounce(my_tracker, my_peer, &saveptr, response);

    assert(status == 0);
    assert(my_peer->listeningPort == 1111);
    assert(my_peer->seededFiles[0] != NULL);
    assert(strcmp(my_peer->seededFiles[0]->key, "8905e92afeb80fc7722ec89eb0bf0966") == 0);
    printf("PASSED\n");

    //bad command
    char bad_raw_command[] = "announce listen 1111 seed [file_a.dat 2097152]";
    first_word = strtok_r(bad_raw_command, " \r\n", &saveptr);
    
    //shouldn't crash
    int bad_status = handleAnnounce(my_tracker, my_peer, &saveptr, response);
    assert(bad_status == -1);
    printf("PASSED\n");

    freeTracker(my_tracker); 
    printf("\n");
}

void test_handleLook() {
    printf("TEST handleLook\n");

    tracker_t* my_tracker = initTracker();
    peer_t* my_peer = addPeer(my_tracker, "192.168.1.50", 2222);

    file_t* file_a = initFile("file_a.dat", 2097152, "8905e92afeb80fc7722ec89eb0bf0966", 1024);
    file_t* file_b = initFile("file_b.dat", 3145728, "330a57722ec8b0bf09669a2b35f88e9e", 1024);
    file_t* file_c = initFile("file_a.dat", 500000, "11111111111111111111111111111111", 1024);

    assert(peerAddSeed(my_peer, file_a) == 0);
    assert(peerAddSeed(my_peer, file_b) == 0);
    assert(peerAddSeed(my_peer, file_c) == 0);

    char response[512]; 
    char good_raw_command[] = "look [filename=file_a.dat filesize>1048576]";
    char* saveptr;
    char* first_word = strtok_r(good_raw_command, " \r\n", &saveptr);

    assert(first_word && strcmp(first_word, "look") == 0);
    int status = handleLook(my_tracker, &saveptr, response);

    assert(status == 0);
    
    assert(strstr(response, "8905e92afeb80fc7722ec89eb0bf0966") != NULL); //file_a is there
    assert(strstr(response, "330a57722ec8b0bf09669a2b35f88e9e") == NULL); //the other two are not
    assert(strstr(response, "11111111111111111111111111111111") == NULL);
    
    printf("PASSED\n");

    freeTracker(my_tracker);
    printf("\n");
}

void test_handleGetfile() {
    printf("TEST handleGetfile\n");

    tracker_t* my_tracker = initTracker();
    
    peer_t* my_peer = addPeer(my_tracker, "192.168.1.50", 2222);
    file_t* file_a = initFile("file_a.dat", 2097152, "8905e92afeb80fc7722ec89eb0bf0966", 1024);
    peerAddSeed(my_peer, file_a);

    char response[512]; 

    char good_raw_command[] = "getfile 8905e92afeb80fc7722ec89eb0bf0966";
    char* saveptr;
    char* first_word = strtok_r(good_raw_command, " \r\n", &saveptr);

    assert(first_word && strcmp(first_word, "getfile") == 0);
    int status = handleGetfile(my_tracker, &saveptr, response);

    assert(status == 0);
    assert(strstr(response, "192.168.1.50") != NULL);
    assert(strstr(response, "2222") != NULL);
    printf("PASSED\n");

    char bad_raw_command[] = "getfile 00000000000000000000000000000000";
    first_word = strtok_r(bad_raw_command, " \r\n", &saveptr);
    
    handleGetfile(my_tracker, &saveptr, response);
    assert(strstr(response, "192.168.1.50") == NULL); //peer doesn't have that key
    printf("PASSEF\n");

    freeTracker(my_tracker);
    printf("\n");
}

int main() {    
    printf("Testing...\n\n");
    
    test_handleAnnounce();
    test_handleLook();    
    test_handleGetfile();
    
    printf("OK\n");
    return 0;
}