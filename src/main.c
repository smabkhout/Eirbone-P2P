#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <string.h>
#include <sys/select.h> 

#include "tracker.h"

#define BUFFER_SIZE 512

void error(const char *msg){
    perror(msg);
    exit(1);
}

int main(int argc, char *argv[]){



    tracker_t* tracker = initTracker();

    if (argc < 2){
        fprintf(stderr, "ERROR, no port provided\n");
        exit(1);
    }

    printf("=========================================\n"
            "     Welcome to Eirbone Application      \n"
           "=========================================\n");


    int client_fds[MAX_PEERS];
    for (int i = 0; i < MAX_PEERS; i++) client_fds[i] = -1;

    int server_fd;
    if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) < 0) 
        error("ERROR opening socket");
    
    struct sockaddr_in serv_addr;
    int portno;
    portno = atoi(argv[1]);
    
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = INADDR_ANY;
    serv_addr.sin_port = htons(portno);

    int opt = 1;
    if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0)
        error("ERROR on setsockopt");

    if (bind(server_fd, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0)
        error("ERROR on binding");

    if (listen(server_fd, 5) < 0)
        error("ERROR on listen");

    printf("Tracker is listening on port %d...\n", portno);

    char buffer[BUFFER_SIZE];
    char answer[BUFFER_SIZE];

    fd_set read_fds;
    int max_fd = server_fd;

    while(1) {
        //at each iteration
        FD_ZERO(&read_fds);
        FD_SET(server_fd, &read_fds);
        max_fd = server_fd;

        for (int i = 0; i < MAX_PEERS; i++) {
            if (client_fds[i] != -1) {
                FD_SET(client_fds[i], &read_fds);
                if (client_fds[i] > max_fd) max_fd = client_fds[i];
            }
        }

        int ready = select(max_fd + 1, &read_fds, NULL, NULL, NULL);
        if (ready < 0) { perror("select"); break; }

        if (FD_ISSET(server_fd, &read_fds)) {
            struct sockaddr_in cl_addr;
            socklen_t cl_len = sizeof(cl_addr);
            int client_fd;
            if ((client_fd = accept(server_fd, (struct sockaddr *)&cl_addr, &cl_len)) < 0)
                error("ERROR on accept");
            
            char *ip = inet_ntoa(cl_addr.sin_addr);
            int slot = -1;
            for (int i = 0; i < MAX_PEERS; i++) {
                if (client_fds[i] == -1) { slot = i; break; }
            }

            if (slot == -1) {
                printf("[TRACKER] Max peers reached, rejecting %s\n", ip);
                close(client_fd);
            } else {
                client_fds[slot] = client_fd;
                tracker->peers[slot] = initPeer(ip, 0);
                printf("[CONNECT] %s assigned to slot %d\n", ip, slot);
            }
        }
        
        for (int i = 0; i < MAX_PEERS; i++) {
            if (client_fds[i] == -1) continue; 
            if (!FD_ISSET(client_fds[i], &read_fds)) continue; 

            memset(buffer, 0, sizeof(buffer));
             
            int b = recv(client_fds[i], buffer, sizeof(buffer) - 1, 0);
            if (b <= 0) {
                printf("[DISCONNECT] slot %d (%s)\n", i, tracker->peers[i]->ipAddr);
                removePeer(tracker, tracker->peers[i]);
                close(client_fds[i]);
                client_fds[i] = -1;
                continue;
            }

            char* saveptr;
            char* cmd = strtok_r(buffer, " \r\n", &saveptr);

            if (cmd == NULL) continue;
        
            if (strcmp(cmd, "announce") == 0)
                handleAnnounce(tracker, tracker->peers[i], &saveptr,  answer);
            else if (strcmp(cmd, "look")     == 0) 
                handleLook(tracker, &saveptr, answer);
            else if (strcmp(cmd, "getfile")  == 0) 
                handleGetfile(tracker, &saveptr, answer);
            // else if (strcmp(cmd, "update")   == 0) 
            //     handleUpdate(tracker, client_peer, &saveptr, answer);
            else {
                sprintf(answer, "error unknown command\n");
                printf("[ERROR] Unknown command from %s: %s\n", tracker->peers[i]->ipAddr, cmd);
            }
            
            send(client_fds[i], answer, strlen(answer), 0);
            memset(answer, 0, sizeof(answer));
        }
    }
    freeTracker(tracker);
    close(server_fd);
    return 0;
}