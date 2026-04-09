#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <string.h>    

#include "tracker.h"

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
    int server_fd;
    if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) < 0) 
        error("ERROR opening socket");
    
    struct sockaddr_in serv_addr;
    int portno;
    portno = atoi(argv[1]);
    
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = INADDR_ANY;
    serv_addr.sin_port = htons(portno);

    if (bind(server_fd, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0)
        error("ERROR on binding");

    listen(server_fd, 5);

    while(1) {
        struct sockaddr_in cl_addr;
        socklen_t cl_len = sizeof(cl_addr);
        int client_fd;
        if ((client_fd = accept(server_fd, (struct sockaddr *)&cl_addr, &cl_len)) < 0)
            error("ERROR on accept");
        
        char *ip = inet_ntoa(cl_addr.sin_addr);
        printf("Client connected: %s\n", ip);
        peer_t* client_peer = addPeer(tracker, ip, 0); //port 0 for now

        
        char buffer[512];
        recv(client_fd, buffer, sizeof(buffer) - 1, 0);
        char* saveptr;
        char* cmd = strtok_r(buffer, " \r\n", &saveptr);

        char answer[512];
    
        if (strcmp(cmd, "announce") == 0)
            handleAnnounce(tracker, client_peer, &saveptr,  answer);
        
        send(client_fd, answer, strlen(answer), 0);
        close(client_fd);
        }

    return 1;
}