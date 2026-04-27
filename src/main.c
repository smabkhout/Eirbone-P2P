#include <arpa/inet.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

#include "tracker.h"
#include "http.h"

#define BUFFER_SIZE 512
#define CLIENT_INBUF_SIZE 4096

void error(const char *msg) {
  perror(msg);
  exit(1);
}

int main(int argc, char *argv[]) {

  tracker_t *tracker = initTracker();

  if (argc < 2) {
    fprintf(stderr, "ERROR, no port provided\n");
    exit(1);
  }

  printf("=========================================\n"
         "     Welcome to Eirbone Application      \n"
         "=========================================\n");

  int client_fds[MAX_PEERS];
  char client_inbuf[MAX_PEERS][CLIENT_INBUF_SIZE];
  size_t client_inbuf_len[MAX_PEERS];
  for (int i = 0; i < MAX_PEERS; i++) {
    client_fds[i] = -1;
    client_inbuf[i][0] = '\0';
    client_inbuf_len[i] = 0;
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

  int opt = 1;
  if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0)
    error("ERROR on setsockopt");

  if (bind(server_fd, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0)
    error("ERROR on binding");

  if (listen(server_fd, 5) < 0)
    error("ERROR on listen");

  /* HTTP dashboard on portno+1 */
  int http_fd;
  int http_port = portno + 1;
  if ((http_fd = socket(AF_INET, SOCK_STREAM, 0)) < 0)
    error("ERROR opening HTTP socket");
  struct sockaddr_in http_addr;
  memset(&http_addr, 0, sizeof(http_addr));
  http_addr.sin_family = AF_INET;
  http_addr.sin_addr.s_addr = INADDR_ANY;
  http_addr.sin_port = htons(http_port);
  int http_opt = 1;
  if (setsockopt(http_fd, SOL_SOCKET, SO_REUSEADDR, &http_opt, sizeof(http_opt)) < 0)
    error("ERROR on http setsockopt");
  if (bind(http_fd, (struct sockaddr *)&http_addr, sizeof(http_addr)) < 0)
    error("ERROR on http binding");
  if (listen(http_fd, 5) < 0)
    error("ERROR on http listen");

  printf("Tracker is listening on port %d...\n", portno);
  printf("Dashboard:  http://localhost:%d\n", http_port);
  char open_cmd[128];
  snprintf(open_cmd, sizeof(open_cmd), "xdg-open 'http://localhost:%d' &", http_port);
  system(open_cmd);

  char buffer[BUFFER_SIZE];
  char answer[BUFFER_SIZE];

  fd_set read_fds;
  int max_fd = server_fd;

  while (1) {
    // at each iteration
    FD_ZERO(&read_fds);
    FD_SET(server_fd, &read_fds);
    FD_SET(http_fd, &read_fds);
    max_fd = server_fd;
    if (http_fd > max_fd) max_fd = http_fd;

    for (int i = 0; i < MAX_PEERS; i++) {
      if (client_fds[i] != -1) {
        FD_SET(client_fds[i], &read_fds);
        if (client_fds[i] > max_fd)
          max_fd = client_fds[i];
      }
    }

    if (select(max_fd + 1, &read_fds, NULL, NULL, NULL) == -1) {
      perror("select");
      break;
    }

    if (FD_ISSET(http_fd, &read_fds)) {
      struct sockaddr_in cl_addr;
      socklen_t cl_len = sizeof(cl_addr);
      int hclient = accept(http_fd, (struct sockaddr *)&cl_addr, &cl_len);
      if (hclient >= 0) {
        serveHttpRequest(tracker, hclient);
        close(hclient);
      }
    }

    if (FD_ISSET(server_fd, &read_fds)) {
      struct sockaddr_in cl_addr;
      socklen_t cl_len = sizeof(cl_addr);
      int client_fd;
      if ((client_fd =
               accept(server_fd, (struct sockaddr *)&cl_addr, &cl_len)) < 0)
        error("ERROR on accept");

      char *ip = inet_ntoa(cl_addr.sin_addr);
      int slot = -1;
      for (int i = 0; i < MAX_PEERS; i++) {
        if (client_fds[i] == -1) {
          slot = i;
          break;
        }
      }

      if (slot == -1) {
        tracker_log("[TRACKER] Max peers reached, rejecting %s\n", ip);
        close(client_fd);
      } else {
        client_fds[slot] = client_fd;
        tracker->peers[slot] = initPeer(ip, 0);
        client_inbuf[slot][0] = '\0';
        client_inbuf_len[slot] = 0;
        tracker_log("[CONNECT] %s assigned to slot %d\n", ip, slot);
      }
    }

    for (int i = 0; i < MAX_PEERS; i++) {
      if (client_fds[i] == -1)
        continue;
      if (!FD_ISSET(client_fds[i], &read_fds))
        continue;

      memset(buffer, 0, sizeof(buffer));
      int b = recv(client_fds[i], buffer, sizeof(buffer) - 1, 0);
      if (b <= 0) {
        if (tracker->peers[i] != NULL) {
          tracker_log("[DISCONNECT] slot %d (%s)\n", i, tracker->peers[i]->ipAddr);
          removePeer(tracker, tracker->peers[i]);
        } else {
          tracker_log("[DISCONNECT] slot %d (unknown)\n", i);
        }
        close(client_fds[i]);
        client_fds[i] = -1;
        client_inbuf[i][0] = '\0';
        client_inbuf_len[i] = 0;
        continue;
      }

      if (client_inbuf_len[i] + (size_t)b >= CLIENT_INBUF_SIZE) {
        tracker_log("[ERROR] Input buffer overflow for slot %d, resetting buffer\n", i);
        client_inbuf_len[i] = 0;
        client_inbuf[i][0] = '\0';
      }

      memcpy(client_inbuf[i] + client_inbuf_len[i], buffer, (size_t)b);
      client_inbuf_len[i] += (size_t)b;
      client_inbuf[i][client_inbuf_len[i]] = '\0';

      char *line_start = client_inbuf[i];
      char *newline;
      while ((newline = strchr(line_start, '\n')) != NULL) {
        *newline = '\0';

        if (*line_start == '\0') {
          line_start = newline + 1;
          continue;
        }

        memset(answer, 0, sizeof(answer));

        char *saveptr;
        char *cmd = strtok_r(line_start, " \r\n", &saveptr);

        if (cmd == NULL) {
          line_start = newline + 1;
          continue;
        }

                if (strcmp(cmd, "announce") == 0)
                    handleAnnounce(tracker, tracker->peers[i], &saveptr,  answer);
                else if (strcmp(cmd, "look")     == 0)
                    handleLook(tracker, &saveptr, answer);
                else if (strcmp(cmd, "getfile")  == 0)
                    handleGetfile(tracker, &saveptr, answer);
                else if (strcmp(cmd, "update")   == 0)
                    handleUpdate(tracker, tracker->peers[i], &saveptr, answer);
                else {
                    sprintf(answer, "error unknown command\n");
                    if (tracker->peers[i] != NULL) {
                        tracker_log("[ERROR] Unknown command from %s: %s\n", tracker->peers[i]->ipAddr, cmd);
                    }
                }

        if (answer[0] != '\0') {
          send(client_fds[i], answer, strlen(answer), 0);
        }

        line_start = newline + 1;
      }

      size_t remaining = strlen(line_start);
      if (line_start != client_inbuf[i]) {
        memmove(client_inbuf[i], line_start, remaining + 1);
      }
      client_inbuf_len[i] = remaining;
    }
  }
  freeTracker(tracker);
  close(server_fd);
  close(http_fd);
  return 0;
}