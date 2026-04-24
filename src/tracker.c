#include "../include/tracker.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

tracker_t *initTracker() {
  tracker_t *tracker = malloc(sizeof(tracker_t));
  if (tracker == NULL) {
    fprintf(stderr, "Erreur d'allocation pour le tracker\n");
    return NULL;
  }
  for (int i = 0; i < MAX_PEERS; i++) {
    tracker->peers[i] = NULL;
  }
  return tracker;
}

void freeTracker(tracker_t *tracker) {
  if (tracker == NULL)
    return;
  for (int i = 0; i < MAX_PEERS; i++) {
    if (tracker->peers[i] != NULL) {
      freePeer(tracker->peers[i]);
      tracker->peers[i] = NULL;
    }
  }
  free(tracker);
}

peer_t *addPeer(tracker_t *tracker, const char *ipAddr, int port) {
  for (int i = 0; i < MAX_PEERS; i++) {
    if (tracker->peers[i] == NULL) {
      tracker->peers[i] = initPeer(ipAddr, port);
      return tracker->peers[i];
    }
  }
  return NULL;
}

int removePeer(tracker_t *tracker, peer_t *peer_to_remove) {
  if (peer_to_remove == NULL)
    return 0;
  for (int i = 0; i < MAX_PEERS; i++) {
    if (tracker->peers[i] == peer_to_remove) {
      freePeer(tracker->peers[i]);
      tracker->peers[i] = NULL;
      return 0;
    }
  }
  return -1;
}

static file_t *findFileByKey(tracker_t *tracker, MD5 key) {
  if (tracker == NULL || key == NULL)
    return NULL;
  for (int i = 0; i < MAX_PEERS; i++) {
    peer_t *current_peer = tracker->peers[i];
    if (current_peer != NULL) {
      file_t **lists[2] = {current_peer->seededFiles,
                           current_peer->leechedFiles};
      for (int l = 0; l < 2; l++) {
        for (int j = 0; j < MAX_FILES; j++) {
          if (lists[l][j] != NULL && strcmp(lists[l][j]->key, key) == 0)
            return lists[l][j];
        }
      }
    }
  }
  return NULL;
}

static void clearPeerFileLists(peer_t* peer) {
    if (peer == NULL) return;

    file_t* unique_files[MAX_FILES * 2];
    int unique_count = 0;

    for (int i = 0; i < MAX_FILES; i++) {
        if (peer->seededFiles[i] != NULL) {
            int seen = 0;
            for (int j = 0; j < unique_count; j++) {
                if (unique_files[j] == peer->seededFiles[i]) {
                    seen = 1;
                    break;
                }
            }
            if (!seen) unique_files[unique_count++] = peer->seededFiles[i];
            peer->seededFiles[i] = NULL;
        }

        if (peer->leechedFiles[i] != NULL) {
            int seen = 0;
            for (int j = 0; j < unique_count; j++) {
                if (unique_files[j] == peer->leechedFiles[i]) {
                    seen = 1;
                    break;
                }
            }
            if (!seen) unique_files[unique_count++] = peer->leechedFiles[i];
            peer->leechedFiles[i] = NULL;
        }
    }

    for (int i = 0; i < unique_count; i++) {
        fileRelease(unique_files[i]);
    }
}

static int collectPeerKnownFiles(peer_t* peer, file_t** out_files, int max_out) {
    if (peer == NULL || out_files == NULL || max_out <= 0) return 0;

    int count = 0;
    for (int i = 0; i < MAX_FILES; i++) {
        file_t* candidates[2] = { peer->seededFiles[i], peer->leechedFiles[i] };
        for (int c = 0; c < 2; c++) {
            file_t* f = candidates[c];
            if (f == NULL) continue;

            int seen = 0;
            for (int j = 0; j < count; j++) {
                if (out_files[j] == f) {
                    seen = 1;
                    break;
                }
            }
            if (!seen && count < max_out) {
                out_files[count++] = f;
                fileRetain(f);
            }
        }
    }
    return count;
}

static file_t* findFileInArrayByKey(file_t** files, int file_count, const char* key) {
    if (files == NULL || key == NULL) return NULL;
    for (int i = 0; i < file_count; i++) {
        if (files[i] != NULL && strcmp(files[i]->key, key) == 0) {
            return files[i];
        }
    }
    return NULL;
}

static int parseSeedList(tracker_t* tracker, peer_t* peer, char **filename, char **saveptr, char* response_buffer){

  char *tmp_filename;
  while ((tmp_filename = strtok_r(NULL, " []]", saveptr)) != NULL) {
    if (strcmp(tmp_filename, "leech") == 0) {
      *filename = tmp_filename;
      break;
    }

    char *length_str = strtok_r(NULL, " []\r\n", saveptr);
    char *piece_str = strtok_r(NULL, " []\r\n", saveptr);
    char *key_str = strtok_r(NULL, " []\r\n", saveptr);

    if (length_str && piece_str && key_str) {
      file_t *exists = findFileByKey(tracker, key_str);
      if (!exists) {
        file_t *f =
            initFile(tmp_filename, atoi(length_str), key_str, atoi(piece_str));
        peerAddSeed(peer, f);
      } else {
        peerAddSeed(peer, exists);
        printf("Fichier %s déjà connu ignoré en seed.\n", key_str);
      }
    } else {
      sprintf(response_buffer,
              "KO: Il manque des informations pour ce fichier\n");
      return -1;
    }
  }
  return 0;
}

static int parseLeechList(tracker_t *tracker, peer_t *peer, char **saveptr) {
  char *key_str;
  while ((key_str = strtok_r(NULL, " []\r\n", saveptr)) != NULL) {
    file_t *existing_file = findFileByKey(tracker, key_str);
    if (existing_file != NULL) {
      peerAddLeech(peer, existing_file);
    } else {
      printf("Log: Fichier %s inconnu ignoré en leech.\n", key_str);
    }
  }
  return 0;
}

int handleAnnounce(tracker_t* tracker, peer_t* current_peer, char** saveptr, char* response_buffer) {
    char* first_kw = strtok_r(NULL, " ", saveptr);
    char* next_kw = first_kw;

    if (first_kw == NULL) return -1;

    if (strcmp(first_kw, "listen") == 0) {
        char* port_str = strtok_r(NULL, " ", saveptr);
        if (port_str == NULL) return -1;
        current_peer->listeningPort = atoi(port_str);
        next_kw = strtok_r(NULL, " ", saveptr);
    }

    clearPeerFileLists(current_peer);

    char* file_name = NULL;
    char* seed_kw = next_kw;
    if (seed_kw && strcmp(seed_kw, "seed") == 0) {
        if (parseSeedList(tracker, current_peer, &file_name, saveptr, response_buffer) != 0) {
            return -1; 
        }
    }
    
    char* leech_kw = NULL;
    if (file_name && strcmp(file_name, "leech") == 0) {
        leech_kw = file_name;
    } else {
        leech_kw = strtok_r(NULL, " [", saveptr);
    }
    if (leech_kw != NULL && strcmp(leech_kw, "leech") == 0) {
        if (parseLeechList(tracker, current_peer, saveptr) != 0) {
            return -1; 
        }   
    }
    sprintf(response_buffer, "ok\n");
    printf("[ANNOUNCE] Peer %s:%d announced\n", current_peer->ipAddr, current_peer->listeningPort);
    return 0;
}

int handleUpdate(tracker_t* tracker, peer_t* current_peer, char** saveptr, char* response_buffer) {
    if (tracker == NULL || current_peer == NULL || saveptr == NULL || response_buffer == NULL) {
        return -1;
    }

    char* seed_kw = strtok_r(NULL, " \r\n", saveptr);
    if (seed_kw == NULL || strcmp(seed_kw, "seed") != 0) {
        sprintf(response_buffer, "KO: missing seed section\n");
        return -1;
    }

    file_t* known_files[MAX_FILES * 2];
    int known_count = collectPeerKnownFiles(current_peer, known_files, MAX_FILES * 2);

    clearPeerFileLists(current_peer);

    char* tok = NULL;
    while ((tok = strtok_r(NULL, " []\r\n", saveptr)) != NULL) {
        if (strcmp(tok, "leech") == 0) {
            break;
        }

        file_t* f = findFileInArrayByKey(known_files, known_count, tok);
        if (f == NULL) {
            f = findFileByKey(tracker, tok);
        }
        if (f != NULL) {
            peerAddSeed(current_peer, f);
        } else {
            printf("[UPDATE] Unknown seed key ignored: %s\n", tok);
        }
    }

    if (tok == NULL || strcmp(tok, "leech") != 0) {
        for (int i = 0; i < known_count; i++) fileRelease(known_files[i]);
        sprintf(response_buffer, "KO: missing leech section\n");
        return -1;
    }

    while ((tok = strtok_r(NULL, " []\r\n", saveptr)) != NULL) {
        file_t* f = findFileInArrayByKey(known_files, known_count, tok);
        if (f == NULL) {
            f = findFileByKey(tracker, tok);
        }
        if (f != NULL) {
            peerAddLeech(current_peer, f);
        } else {
            printf("[UPDATE] Unknown leech key ignored: %s\n", tok);
        }
    }

    for (int i = 0; i < known_count; i++) fileRelease(known_files[i]);

    sprintf(response_buffer, "ok\n");
    printf("[UPDATE] Peer %s:%d updated state\n", current_peer->ipAddr, current_peer->listeningPort);
    return 0;
}

static void parseLookCriteria(char** saveptr, char* target_filename, int* target_filesize) {
    char* criterion;
    while ((criterion = strtok_r(NULL, " []\r\n", saveptr)) != NULL) {
        if (strncmp(criterion, "filename=", 9) == 0) {
            strncpy(target_filename, criterion + 9, MAX_FILENAME - 1);
        } else if (strncmp(criterion, "filesize>", 9) == 0) {
            *target_filesize = atoi(criterion + 9);
        }
    }
}

static int isFileDuplicate(file_t **found_files, int found_count, char *key) {
  for (int k = 0; k < found_count; k++) {
    if (strcmp(found_files[k]->key, key) == 0) {
      return 1;
    }
  }
  return 0;
}

static int fileMatchesCriteria(file_t *f, char *target_filename,
                               int target_filesize) {
  if (target_filename[0] != '\0' && strcmp(f->filename, target_filename) != 0)
    return 0;
  if (target_filesize != -1 && f->length <= target_filesize)
    return 0;
  return 1;
}

static void searchFilesInNetwork(tracker_t *tracker, char *target_filename,
                                 int target_filesize, file_t **found_files,
                                 int *found_count) {
  for (int i = 0; i < MAX_PEERS; i++) {
    peer_t *current_peer = tracker->peers[i];
    if (current_peer == NULL)
      continue;

    file_t **list_to_check[2] = {current_peer->seededFiles,
                                 current_peer->leechedFiles};

    for (int list_idx = 0; list_idx < 2; list_idx++) {
      file_t **current_list = list_to_check[list_idx];

      for (int j = 0; j < MAX_FILES; j++) {
        file_t *f = current_list[j];
        if (f == NULL)
          continue;
        if (fileMatchesCriteria(f, target_filename, target_filesize) &&
            !isFileDuplicate(found_files, *found_count, f->key)) {
          if (*found_count < 100) {
            found_files[*found_count] = f;
            (*found_count)++;
          }
        }
      }
    }
  }
}

static void formatLookResponse(file_t **found_files, int found_count,
                               char *response_buffer) {
  strcpy(response_buffer, "list [");

  for (int i = 0; i < found_count; i++) {
    char file_info[256];
    sprintf(file_info, "%s %d %d %s", found_files[i]->filename,
            found_files[i]->length, found_files[i]->piece_size,
            found_files[i]->key);

    strcat(response_buffer, file_info);
    if (i < found_count - 1) {
      strcat(response_buffer, " ");
    }
  }

  strcat(response_buffer, "]\n");
}

int handleLook(tracker_t *tracker, char **saveptr, char *response_buffer) {
  char target_filename[MAX_FILENAME] = {0};
  int target_filesize = -1;

  file_t *found_files[100];
  int found_count = 0;

  parseLookCriteria(saveptr, target_filename, &target_filesize);
  searchFilesInNetwork(tracker, target_filename, target_filesize, found_files,
                       &found_count);
  formatLookResponse(found_files, found_count, response_buffer);

  printf("[LOOK] criteria: filename=%s filesize>%d: %d result(s)\n",
         target_filename, target_filesize, found_count);

  return 0;
}

int handleGetfile(tracker_t *tracker, char **saveptr, char *response_buffer) {
  char *key_str = strtok_r(NULL, " \r\n", saveptr);

  if (key_str == NULL)
    return -1;

  sprintf(response_buffer, "peers %s [", key_str);

  int first_peer_added = 1;
  int peer_count = 0;

  for (int i = 0; i < MAX_PEERS; i++) {
    peer_t *current_peer = tracker->peers[i];

    if (current_peer != NULL) {

      if (peerRequestFile(current_peer, key_str) != NONE) {
        peer_count++;

        if (!first_peer_added) {
          strcat(response_buffer, " ");
        }

        char peer_info[64];
        sprintf(peer_info, "%s:%d", current_peer->ipAddr,
                current_peer->listeningPort);

        strcat(response_buffer, peer_info);

        first_peer_added = 0;
      }
    }
  }
  strcat(response_buffer, "]\n");

  printf("[GETFILE] key=%s: %d peer(s) found\n", key_str, peer_count);

  return 0;
}