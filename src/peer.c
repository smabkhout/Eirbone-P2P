#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../include/peer.h"

peer_t *initPeer(const char *ipAddr, int listeningPort) {
  struct peer_t *p = malloc(sizeof(struct peer_t));
  p->listeningPort = listeningPort;
  strncpy(p->ipAddr, ipAddr, ADDRESS_LEN - 1);
  p->ipAddr[ADDRESS_LEN - 1] = '\0';
  for (int i = 0; i < MAX_FILES; i++) {
    p->seededFiles[i] = NULL;
    p->leechedFiles[i] = NULL;
  }
  return p;
}

int peerAddSeed(peer_t *peer, file_t *file) {
  if (peer == NULL || file == NULL)
    return -1;
  for (int i = 0; i < MAX_FILES; i++) {
    if (peer->seededFiles[i] == file)
      return 0;
  }
  for (int i = 0; i < MAX_FILES; i++) {
    if (!peer->seededFiles[i]) {
      peer->seededFiles[i] = file;
      fileRetain(file);
      return 0;
    }
  }
  return -1;
}

int peerAddLeech(peer_t *peer, file_t *file) {
  if (peer == NULL || file == NULL)
    return -1;
  for (int i = 0; i < MAX_FILES; i++) {
    if (peer->leechedFiles[i] == file)
      return 0;
  }
  for (int i = 0; i < MAX_FILES; i++) {
    if (!peer->leechedFiles[i]) {
      peer->leechedFiles[i] = file;
      fileRetain(file);

      return 0;
    }
  }
  return -1;
}

enum fileType peerRequestFile(peer_t *peer, MD5 fileKey) {
  if (peer == NULL || fileKey == NULL)
    return NONE;

  for (int i = 0; i < MAX_FILES; i++) {

    if (peer->leechedFiles[i] != NULL) {
      if (strcmp(peer->leechedFiles[i]->key, fileKey) == 0) {
        return LEECHER;
      }
    }

    if (peer->seededFiles[i] != NULL) {
      if (strcmp(peer->seededFiles[i]->key, fileKey) == 0) {
        return SEEDER;
      }
    }
  }
  return NONE;
}

void freePeer(peer_t *peer) {
  if (peer == NULL)
    return;

  file_t *unique_files[MAX_FILES * 2];
  int unique_count = 0;

  for (int i = 0; i < MAX_FILES; i++) {
    if (peer->leechedFiles[i]) {
      int seen = 0;
      for (int j = 0; j < unique_count; j++) {
        if (unique_files[j] == peer->leechedFiles[i]) {
          seen = 1;
          break;
        }
      }
      if (!seen) {
        unique_files[unique_count++] = peer->leechedFiles[i];
      }
      peer->leechedFiles[i] = NULL;
    }
    if (peer->seededFiles[i]) {
      int seen = 0;
      for (int j = 0; j < unique_count; j++) {
        if (unique_files[j] == peer->seededFiles[i]) {
          seen = 1;
          break;
        }
      }
      if (!seen) {
        unique_files[unique_count++] = peer->seededFiles[i];
      }
      peer->seededFiles[i] = NULL;
    }
  }

  for (int i = 0; i < unique_count; i++) {
    fileRelease(unique_files[i]);
  }

  free(peer);
}