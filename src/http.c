#include "../include/http.h"
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <sys/socket.h>

/* ── circular log buffer ───────────────────────────────────────────────── */

#define LOG_MAX  50
#define LOG_LINE 200

static char log_buf[LOG_MAX][LOG_LINE];
static int  log_head  = 0;
static int  log_count = 0;

void tracker_log(const char *fmt, ...) {
    va_list ap, ap2;
    va_start(ap, fmt);
    va_copy(ap2, ap);

    vprintf(fmt, ap);
    fflush(stdout);
    va_end(ap);

    time_t now = time(NULL);
    struct tm *t = localtime(&now);
    char ts[10];
    strftime(ts, sizeof(ts), "%H:%M:%S", t);

    char msg[LOG_LINE - 12];
    vsnprintf(msg, sizeof(msg), fmt, ap2);
    va_end(ap2);

    int len = (int)strlen(msg);
    if (len > 0 && msg[len - 1] == '\n') msg[--len] = '\0';

    snprintf(log_buf[log_head], LOG_LINE, "[%s] %s", ts, msg);
    log_head = (log_head + 1) % LOG_MAX;
    if (log_count < LOG_MAX) log_count++;
}

/* ── static file serving ───────────────────────────────────────────────── */

static void send_404(int fd) {
    const char *r = "HTTP/1.1 404 Not Found\r\n"
                    "Content-Length: 9\r\n"
                    "Connection: close\r\n\r\nNot Found";
    send(fd, r, strlen(r), 0);
}

static void serve_file(int fd, const char *path, const char *mime) {
    FILE *f = fopen(path, "rb");
    if (!f) { send_404(fd); return; }

    fseek(f, 0, SEEK_END);
    long sz = ftell(f);
    rewind(f);

    char *body = malloc((size_t)sz);
    if (!body) { fclose(f); send_404(fd); return; }
    fread(body, 1, (size_t)sz, f);
    fclose(f);

    char hdr[256];
    snprintf(hdr, sizeof(hdr),
        "HTTP/1.1 200 OK\r\n"
        "Content-Type: %s\r\n"
        "Content-Length: %ld\r\n"
        "Connection: close\r\n\r\n",
        mime, sz);
    send(fd, hdr, strlen(hdr), 0);
    send(fd, body, (size_t)sz, 0);
    free(body);
}

/* ── JSON builder ──────────────────────────────────────────────────────── */

static void build_json(tracker_t *tracker, char *buf, size_t bufsz) {
    int off = 0;
    off += snprintf(buf + off, bufsz - (size_t)off, "{\"peers\":[");

    int first = 1;
    for (int i = 0; i < MAX_PEERS; i++) {
        peer_t *p = tracker->peers[i];
        if (!p || p->listeningPort == 0) continue;

        int ns = 0, nl = 0;
        for (int j = 0; j < MAX_FILES; j++) {
            if (p->seededFiles[j])  ns++;
            if (p->leechedFiles[j]) nl++;
        }

        const char *status = (ns > 0 && nl > 0) ? "both"
                           : (ns > 0)            ? "seeding"
                           : (nl > 0)            ? "leeching"
                           :                       "standby";

        if (!first && off < (int)bufsz - 1) buf[off++] = ',';
        first = 0;
        off += snprintf(buf + off, bufsz - (size_t)off,
            "{\"ip\":\"%s\",\"port\":%d,\"seeding\":%d,\"leeching\":%d,\"status\":\"%s\"}",
            p->ipAddr, p->listeningPort, ns, nl, status);
    }

    off += snprintf(buf + off, bufsz - (size_t)off, "],\"log\":[");

    /* emit log entries oldest → newest; JS will reverse for display */
    int start = (log_count < LOG_MAX) ? 0 : log_head;
    first = 1;
    for (int k = 0; k < log_count; k++) {
        const char *s = log_buf[(start + k) % LOG_MAX];
        if (!first && off < (int)bufsz - 1) buf[off++] = ',';
        first = 0;

        /* inline JSON string escape */
        char esc[LOG_LINE * 2 + 4];
        int ei = 0;
        for (int si = 0; s[si] && ei < (int)sizeof(esc) - 3; si++) {
            if (s[si] == '"' || s[si] == '\\') esc[ei++] = '\\';
            esc[ei++] = s[si];
        }
        esc[ei] = '\0';
        off += snprintf(buf + off, bufsz - (size_t)off, "\"%s\"", esc);
    }

    snprintf(buf + off, bufsz - (size_t)off, "]}");
}

/* ── request handler ───────────────────────────────────────────────────── */

void serveHttpRequest(tracker_t *tracker, int fd) {
    char req[1024] = {0};
    int n = recv(fd, req, sizeof(req) - 1, 0);
    if (n <= 0) return;
    req[n] = '\0';

    char path[256] = "/";
    sscanf(req, "%*s %255s", path);

    if (strcmp(path, "/api/state") == 0) {
        static char json[32768];
        build_json(tracker, json, sizeof(json));
        char hdr[256];
        snprintf(hdr, sizeof(hdr),
            "HTTP/1.1 200 OK\r\n"
            "Content-Type: application/json\r\n"
            "Access-Control-Allow-Origin: *\r\n"
            "Content-Length: %zu\r\n"
            "Connection: close\r\n\r\n",
            strlen(json));
        send(fd, hdr, strlen(hdr), 0);
        send(fd, json, strlen(json), 0);
    } else if (strcmp(path, "/tracker.css") == 0) {
        serve_file(fd, "www/tracker.css", "text/css");
    } else if (strcmp(path, "/tracker.js") == 0) {
        serve_file(fd, "www/tracker.js", "application/javascript");
    } else {
        serve_file(fd, "www/tracker.html", "text/html; charset=utf-8");
    }
}
