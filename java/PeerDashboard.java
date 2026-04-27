import com.sun.net.httpserver.HttpExchange;
import com.sun.net.httpserver.HttpServer;
import java.io.*;
import java.net.InetSocketAddress;
import java.nio.file.Files;
import java.nio.file.Paths;
import java.util.Base64;
import java.util.Map;

public class PeerDashboard {

    private final Peer peer;
    private final String wwwDir;

    public PeerDashboard(Peer peer) {
        this.peer = peer;
        // Works whether run from project root or from java/ subdirectory
        File f = new File("../www");
        this.wwwDir = (f.exists() && f.isDirectory()) ? "../www" : "www";
    }

    /** Starts the HTTP server on peer.getPort()+1 and returns that port. */
    public int start() throws IOException {
        int dashPort = peer.getPort() + 1;
        HttpServer server = HttpServer.create(new InetSocketAddress(dashPort), 10);
        server.createContext("/api/state", this::handleApi);
        server.createContext("/",          this::handleStatic);
        server.setExecutor(null);
        server.start();
        return dashPort;
    }

    private void handleApi(HttpExchange ex) throws IOException {
        byte[] body = buildJson().getBytes("UTF-8");
        ex.getResponseHeaders().set("Content-Type", "application/json");
        ex.getResponseHeaders().set("Access-Control-Allow-Origin", "*");
        ex.sendResponseHeaders(200, body.length);
        try (OutputStream out = ex.getResponseBody()) { out.write(body); }
    }

    private void handleStatic(HttpExchange ex) throws IOException {
        String path = ex.getRequestURI().getPath();
        String file; String mime;

        if (path.equals("/") || path.equals("/index.html")) {
            file = wwwDir + "/peer.html"; mime = "text/html; charset=utf-8";
        } else if (path.equals("/peer.css")) {
            file = wwwDir + "/peer.css"; mime = "text/css";
        } else if (path.equals("/peer.js")) {
            file = wwwDir + "/peer.js"; mime = "application/javascript";
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

    private String buildJson() {
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
                int pieces = (int) ((fm.getSize() + 1023) / 1024);
                sb.append("{\"filename\":\"").append(escJson(fm.getFileName())).append("\"");
                sb.append(",\"size\":").append(fm.getSize());
                sb.append(",\"md5\":\"").append(fm.getHash()).append("\"");
                sb.append(",\"pieces\":").append(pieces);
                sb.append(",\"partial\":false}");
            } else if (fm.getState() == FileState.LEECH) {
                int totalPieces = (int) ((fm.getSize() + 1023) / 1024);
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
            int totalPieces = (int) ((fm.getSize() + 1023) / 1024);
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
                int byteIdx = i / 8, bitIdx = 7 - (i % 8);
                sb.append(byteIdx < bits.length && (bits[byteIdx] & (1 << bitIdx)) != 0 ? '1' : '0');
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
