# Eirbone — peer-to-peer file sharing

A BitTorrent-style P2P file-sharing application built for the *MA-CHRO3* networking project at ENSEIRB-MATMECA (2A / S8). Large files are split into fixed-size pieces (1 KiB) identified by their MD5 hash, and peers exchange those pieces directly with each other while a central **tracker** keeps track of who is serving what.

The system has two halves that talk over a simple text-based TCP protocol:

| Side | Language | Role |
|---|---|---|
| **Tracker** | C (single binary `tracker`) | Central directory: handles `announce`, `look`, `getfile`, `update` from peers and exposes a live HTTP dashboard. |
| **Peer** | Java | Connects to the tracker, listens for other peers, runs the piece-exchange protocol (`interested` / `have` buffermaps, `getpieces`), and exposes its own HTTP dashboard. |

## How it works

- **Tracker (`src/`, `include/`)** — single-threaded `select()` loop on the listening port (12345 by default), with up to `MAX_PEERS` simultaneous connections. Each peer has a slot holding the files it seeds and the files it leeches. A separate HTTP server on `port + 1` serves `www/tracker.{html,css,js}` so you can watch the swarm live in a browser.
- **Peer (`java/`)** — picks a random TCP port, registers with the tracker (`announce`), then accepts incoming peer connections in a daemon thread. Periodic background threads refresh the tracker (`update`) and exchange buffermaps with known peers. Buffermaps are base64-encoded bitsets where bit `i` means "I have piece `i`". Pieces requested with `getpieces` are answered as binary blobs and reassembled into the target file.
- **Configuration** — `config.ini` controls the tracker address/port, refresh intervals and log level. Both sides read it on startup.
- **Dashboards** — the tracker shows the current swarm; each peer shows its files, buffermap and known peers. Both open automatically via `xdg-open` on startup.

## CLI commands (peer)

```
look <filename>              search for a file on the tracker
getfile <key>                ask the tracker which peers have a file
interested <port> <key>      ask another peer for its buffermap
getpieces <port> <key> <i…>  request specific pieces from another peer
getpieces <port> <key> :     request every piece the peer has
exit | q                     quit
```

## Layout

```
free-razrezo/
├── src/, include/        # C tracker: tracker.{c,h}, peer.{c,h}, file.{c,h}, http.{c,h}, main.c
├── java/                 # Java peer: Main, Peer, PeerDiscovery, PeerDashboard,
│                         # FileMetadata, RemotePeer, AppConfig, AppLogger, tests
├── www/                  # HTML/CSS/JS for the tracker and peer dashboards
├── test/                 # C handler tests
├── rapport/              # Final LaTeX report (rapport_final.tex)
├── rapport_final.pdf
├── config.ini            # tracker address/port, intervals, log level
└── Makefile
```

## Build & run

```bash
make c                              # build the tracker
make java                           # compile the peer

./tracker                           # uses config.ini  (also opens the tracker dashboard)
./tracker 12345                     # or pass an explicit port

make run-java                       # launch a peer
make run-java ARGS=/path/to/file    # launch a peer that already seeds a file
make test-c                         # build the C handler tests
make test-java                      # run PeerProtocolTest
make rapport                        # build the PDF report
make clean
```

Authors: MABKHOUT Salah-Eddine, MIGAOU Mohamed Aziz, BENBAKI Anas, DAMMOU Adam — ENSEIRB-MATMECA 2025-2026.
