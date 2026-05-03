import com.sun.net.httpserver.HttpExchange;
import com.sun.net.httpserver.HttpServer;
import java.io.*;
import java.net.InetSocketAddress;
import java.net.URLDecoder;
import java.nio.charset.StandardCharsets;
import java.nio.file.Files;
import java.nio.file.Paths;
import java.util.ArrayList;
import java.util.Base64;
import java.util.HashMap;
import java.util.List;
import java.util.Map;

public class PeerDashboard {

    private final Peer peer;
    private final String wwwDir;

    public PeerDashboard(Peer peer) {
        this.peer = peer;
        File f = new File("../www");
        this.wwwDir = (f.exists() && f.isDirectory()) ? "../www" : "www";
    }

    public int start() throws IOException {
        int dashPort = peer.getPort() + 1;
        HttpServer server = HttpServer.create(new InetSocketAddress(dashPort), 10);
        server.createContext("/api/state",      this::handleState);
        server.createContext("/api/look",       this::handleLook);
        server.createContext("/api/getfile",    this::handleGetFile);
        server.createContext("/api/download",   this::handleDownload);
        server.createContext("/api/seed",       this::handleSeedUpload);
        server.createContext("/api/interested", this::handleInterested);
        server.createContext("/api/getpieces",  this::handleGetPieces);
        server.createContext("/",               this::handleStatic);
        server.setExecutor(null);
        server.start();
        return dashPort;
    }

    /* ── /api/state ─────────────────────────────────────────────────────── */

    private void handleState(HttpExchange ex) throws IOException {
        sendJson(ex, buildStateJson());
    }

    /* ── /api/look?q=<filename> ──────────────────────────────────────────── */

    private void handleLook(HttpExchange ex) throws IOException {
        Map<String, String> params = parseQuery(ex.getRequestURI().getQuery());
        String q = params.getOrDefault("q", "").trim();
        String json = q.isEmpty() ? "{\"files\":[]}" : buildLookJson(peer.lookRaw(q));
        sendJson(ex, json);
    }

    private String buildLookJson(String raw) {
        StringBuilder sb = new StringBuilder("{\"files\":[");
        if (raw != null && raw.startsWith("list [")) {
            int s = raw.indexOf('[') + 1, e = raw.lastIndexOf(']');
            if (e > s) {
                String[] tok = raw.substring(s, e).trim().split("\\s+");
                boolean first = true;
                for (int i = 0; i + 4 <= tok.length; i += 4) {
                    try {
                        long size = Long.parseLong(tok[i + 1]);
                        int  ps   = Integer.parseInt(tok[i + 2]);
                        int  pcs  = (int)((size + ps - 1) / ps);
                        if (!first) sb.append(",");
                        first = false;
                        sb.append("{\"filename\":\"").append(escJson(tok[i])).append("\"");
                        sb.append(",\"size\":").append(size);
                        sb.append(",\"pieces\":").append(pcs);
                        sb.append(",\"md5\":\"").append(tok[i + 3]).append("\"}");
                    } catch (NumberFormatException ignored) { break; }
                }
            }
        }
        return sb.append("]}").toString();
    }

    /* ── /api/getfile?key=<md5> ──────────────────────────────────────────── */

    private void handleGetFile(HttpExchange ex) throws IOException {
        Map<String, String> params = parseQuery(ex.getRequestURI().getQuery());
        String key = params.getOrDefault("key", "").trim();
        String json = key.isEmpty() ? "{\"peers\":[]}" : buildGetFileJson(peer.getFileRaw(key));
        sendJson(ex, json);
    }

    private String buildGetFileJson(String raw) {
        StringBuilder sb = new StringBuilder("{\"peers\":[");
        if (raw != null && raw.startsWith("peers ")) {
            int s = raw.indexOf('[') + 1, e = raw.lastIndexOf(']');
            if (e > s) {
                boolean first = true;
                for (String tok : raw.substring(s, e).trim().split("\\s+")) {
                    if (tok.isEmpty()) continue;
                    String[] ip = tok.split(":", 2);
                    if (ip.length != 2) continue;
                    try {
                        if (!first) sb.append(",");
                        first = false;
                        sb.append("{\"ip\":\"").append(ip[0]).append("\"");
                        sb.append(",\"port\":").append(Integer.parseInt(ip[1])).append("}");
                    } catch (NumberFormatException ignored) {}
                }
            }
        }
        return sb.append("]}").toString();
    }

    /* ── /api/download?key=<md5>&port=<n> ───────────────────────────────── */

    private void handleDownload(HttpExchange ex) throws IOException {
        Map<String, String> params = parseQuery(ex.getRequestURI().getQuery());
        String key   = params.getOrDefault("key",  "").trim();
        String portS = params.getOrDefault("port", "").trim();
        String json  = "{\"status\":\"error\"}";
        if (!key.isEmpty() && !portS.isEmpty()) {
            try {
                peer.startDownload(key, Integer.parseInt(portS));
                json = "{\"status\":\"started\"}";
            } catch (NumberFormatException ignored) {}
        }
        sendJson(ex, json);
    }

    /* ── /api/seed?name=<filename>&path=<target> ───────────────────────── */

    private void handleSeedUpload(HttpExchange ex) throws IOException {
        if (!"post".equalsIgnoreCase(ex.getRequestMethod())) {
            sendJson(ex, "{\"status\":\"error\"}");
            return;
        }

        Map<String, String> params = parseQuery(ex.getRequestURI().getQuery());
        String name = params.getOrDefault("name", "").trim();
        // Ignore any path provided by the user, always store in peer's directory
        if (name.isEmpty()) {
            sendJson(ex, "{\"status\":\"error\",\"message\":\"missing filename\"}");
            return;
        }

        byte[] content = ex.getRequestBody().readAllBytes();
        if (content.length == 0) {
            sendJson(ex, "{\"status\":\"error\",\"message\":\"empty file\"}");
            return;
        }

        try {
            FileMetadata fm = peer.addSeedFile(name, null, content);
            // Envoie un announce pour que le tracker connaisse le nouveau fichier
            if (peer != null) {
                String announceReq = peer.buildAnnounceRequest();
                System.out.println("Annonce envoyée au tracker pour le fichier ajouté.");
                peer.sendTrackerRequest(announceReq);
            }
            peer.refreshTrackerNow();
            StringBuilder sb = new StringBuilder();
            sb.append("{\"status\":\"started\"");
            sb.append(",\"filename\":\"").append(escJson(fm.getFileName())).append("\"");
            sb.append(",\"md5\":\"").append(fm.getHash()).append("\"");
            sb.append(",\"size\":").append(fm.getSize());
            sb.append(",\"path\":\"").append(escJson(fm.getlocalPath().getAbsolutePath())).append("\"}");
            sendJson(ex, sb.toString());
        } catch (IllegalArgumentException e) {
            sendJson(ex, "{\"status\":\"error\",\"message\":\"invalid file\"}");
        }
    }

    /* ── /api/interested?key=<md5>&port=<n> ────────────────────────────── */

    private void handleInterested(HttpExchange ex) throws IOException {
        Map<String, String> params = parseQuery(ex.getRequestURI().getQuery());
        String key  = params.getOrDefault("key",  "").trim();
        String portS = params.getOrDefault("port", "").trim();
        if (key.isEmpty() || portS.isEmpty()) {
            sendJson(ex, "{\"bitmap\":\"\"}");
            return;
        }
        try {
            int port = Integer.parseInt(portS);
            String resp = peer.requestInterested(port, key);
            String bitmap = "";
            if (resp != null && resp.startsWith("have ")) {
                String[] parts = resp.split("\\s+", 3);
                if (parts.length == 3) bitmap = decodeBitmapToString(parts[2]);
            }
            sendJson(ex, "{\"bitmap\":\"" + bitmap + "\"}");
        } catch (NumberFormatException e) {
            sendJson(ex, "{\"bitmap\":\"\"}");
        }
    }

    /* ── /api/getpieces?key=<md5>&port=<n>&indexes=0,2,5 ────────────── */

    private void handleGetPieces(HttpExchange ex) throws IOException {
        Map<String, String> params = parseQuery(ex.getRequestURI().getQuery());
        String key    = params.getOrDefault("key",     "").trim();
        String portS  = params.getOrDefault("port",    "").trim();
        String idxStr = params.getOrDefault("indexes", "").trim();
        if (key.isEmpty() || portS.isEmpty() || idxStr.isEmpty()) {
            sendJson(ex, "{\"status\":\"error\"}");
            return;
        }
        try {
            int port = Integer.parseInt(portS);
            List<Integer> indexes = new ArrayList<>();
            for (String s : idxStr.split(",")) {
                s = s.trim();
                if (!s.isEmpty()) indexes.add(Integer.parseInt(s));
            }
            if (indexes.isEmpty()) { sendJson(ex, "{\"status\":\"error\"}"); return; }
            peer.downloadPieces(key, port, indexes);
            sendJson(ex, "{\"status\":\"started\"}");
        } catch (NumberFormatException e) {
            sendJson(ex, "{\"status\":\"error\"}");
        }
    }

    private String decodeBitmapToString(String base64) {
        try {
            byte[] bits = Base64.getDecoder().decode(base64);
            StringBuilder sb = new StringBuilder(bits.length * 8);
            for (int i = 0; i < bits.length * 8; i++) {
                int by = i / 8, bi = 7 - (i % 8);
                sb.append((bits[by] & (1 << bi)) != 0 ? '1' : '0');
            }
            String full = sb.toString();
            int last1 = full.lastIndexOf('1');
            return last1 >= 0 ? full.substring(0, last1 + 1) : "";
        } catch (IllegalArgumentException e) {
            return "";
        }
    }

    /* ── static files ────────────────────────────────────────────────────── */

    private void handleStatic(HttpExchange ex) throws IOException {
        String path = ex.getRequestURI().getPath();
        String file, mime;
        if (path.equals("/") || path.equals("/index.html")) {
            file = wwwDir + "/peer.html"; mime = "text/html; charset=utf-8";
        } else if (path.equals("/peer.css")) {
            file = wwwDir + "/peer.css";  mime = "text/css";
        } else if (path.equals("/peer.js")) {
            file = wwwDir + "/peer.js";   mime = "application/javascript";
        } else {
            ex.sendResponseHeaders(404, 0);
            ex.getResponseBody().close();
            return;
        }
        try {
            byte[] body = Files.readAllBytes(Paths.get(file));
            ex.getResponseHeaders().set("Content-Type", mime);
            ex.sendResponseHeaders(200, body.length);
            try (OutputStream out = ex.getResponseBody()) { out.write(body); }
        } catch (IOException e) {
            ex.sendResponseHeaders(404, 0);
            ex.getResponseBody().close();
        }
    }

    /* ── helpers ─────────────────────────────────────────────────────────── */

    private void sendJson(HttpExchange ex, String json) throws IOException {
        byte[] body = json.getBytes(StandardCharsets.UTF_8);
        ex.getResponseHeaders().set("Content-Type", "application/json");
        ex.getResponseHeaders().set("Access-Control-Allow-Origin", "*");
        ex.sendResponseHeaders(200, body.length);
        try (OutputStream out = ex.getResponseBody()) { out.write(body); }
    }

    private Map<String, String> parseQuery(String query) {
        Map<String, String> map = new HashMap<>();
        if (query == null || query.isEmpty()) return map;
        for (String pair : query.split("&")) {
            String[] kv = pair.split("=", 2);
            if (kv.length == 2) {
                try {
                    map.put(URLDecoder.decode(kv[0], "UTF-8"),
                            URLDecoder.decode(kv[1], "UTF-8"));
                } catch (IOException ignored) {}
            }
        }
        return map;
    }

    /* ── state JSON ──────────────────────────────────────────────────────── */

    private String buildStateJson() {
        Map<String, FileMetadata> files = peer.getManagedFiles();
        StringBuilder sb = new StringBuilder();
        sb.append("{\"ip\":\"").append(peer.getIpAddress()).append("\"");
        sb.append(",\"port\":").append(peer.getPort());

        sb.append(",\"seeding\":[");
        boolean first = true;
        for (FileMetadata fm : files.values()) {
            if (fm.getState() == FileState.SEED) {
                if (!first) sb.append(",");
                first = false;
                int pieces = (int)((fm.getSize() + 1023) / 1024);
                sb.append("{\"filename\":\"").append(escJson(fm.getFileName())).append("\"");
                sb.append(",\"size\":").append(fm.getSize());
                sb.append(",\"md5\":\"").append(fm.getHash()).append("\"");
                sb.append(",\"pieces\":").append(pieces);
                sb.append(",\"partial\":false}");
            } else if (fm.getState() == FileState.LEECH) {
                int totalPieces = (int)((fm.getSize() + 1023) / 1024);
                String bitmap = toBitmapString(fm, totalPieces);
                long got = bitmap.chars().filter(c -> c == '1').count();
                if (got == 0) continue;
                if (!first) sb.append(",");
                first = false;
                boolean partial = got < totalPieces;
                sb.append("{\"filename\":\"").append(escJson(fm.getFileName())).append("\"");
                sb.append(",\"size\":").append(fm.getSize());
                sb.append(",\"md5\":\"").append(fm.getHash()).append("\"");
                sb.append(",\"pieces\":").append(partial ? got : totalPieces);
                sb.append(",\"partial\":").append(partial).append("}");
            }
        }

        sb.append("],\"leeching\":[");
        first = true;
        for (FileMetadata fm : files.values()) {
            if (fm.getState() != FileState.LEECH) continue;
            if (!first) sb.append(",");
            first = false;
            int totalPieces = (int)((fm.getSize() + 1023) / 1024);
            String bitmap = toBitmapString(fm, totalPieces);
            long got = bitmap.chars().filter(c -> c == '1').count();
            sb.append("{\"filename\":\"").append(escJson(fm.getFileName())).append("\"");
            sb.append(",\"size\":").append(fm.getSize());
            sb.append(",\"md5\":\"").append(fm.getHash()).append("\"");
            sb.append(",\"totalPieces\":").append(totalPieces);
            sb.append(",\"gotPieces\":").append(got);
            sb.append(",\"bitmap\":\"").append(bitmap).append("\"}");
        }

        sb.append("]}");
        return sb.toString();
    }

    private String toBitmapString(FileMetadata fm, int totalPieces) {
        String b64 = fm.getBufferMap();
        if (b64 == null || b64.isEmpty()) return "0".repeat(totalPieces);
        try {
            byte[] bits = Base64.getDecoder().decode(b64);
            StringBuilder sb = new StringBuilder(totalPieces);
            for (int i = 0; i < totalPieces; i++) {
                int by = i / 8, bi = 7 - (i % 8);
                sb.append(by < bits.length && (bits[by] & (1 << bi)) != 0 ? '1' : '0');
            }
            return sb.toString();
        } catch (IllegalArgumentException e) {
            return "0".repeat(totalPieces);
        }
    }

    private String escJson(String s) {
        return s.replace("\\", "\\\\").replace("\"", "\\\"");
    }
}
