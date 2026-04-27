#ifndef HTTP_H
#define HTTP_H

#include "tracker.h"

void tracker_log(const char *fmt, ...);
void serveHttpRequest(tracker_t *tracker, int client_fd);

#endif // HTTP_H
