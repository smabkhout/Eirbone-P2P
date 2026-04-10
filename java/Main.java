import java.net.ServerSocket;
import java.util.Scanner;
import java.util.ArrayList;
import java.util.List;
import java.io.File;
import java.io.FileInputStream;
import java.security.MessageDigest;

public class Main {
    private static String md5Hex(File file) throws Exception {
        MessageDigest md = MessageDigest.getInstance("MD5");
        try (FileInputStream in = new FileInputStream(file)) {
            byte[] buffer = new byte[8192];
            int read;
            while ((read = in.read(buffer)) != -1) {
                md.update(buffer, 0, read);
            }
        }

        byte[] digest = md.digest();
        StringBuilder sb = new StringBuilder();
        for (byte b : digest) {
            sb.append(String.format("%02x", b));
        }
        return sb.toString();
    }

    public static void main(String[] args) {
        System.out.println("=========================================");
        System.out.println("     Welcome to Eirbone Application      ");
        System.out.println("=========================================");

        try {
            int autoPort;
            ServerSocket s = new ServerSocket(0);
            autoPort = s.getLocalPort();
            s.close();

            String filePath = args.length > 0 ? args[0] : "/tmp/file_a.dat";
            File seedFile = new File(filePath);

            String seedKey;
            long seedSize;
            if (seedFile.exists() && seedFile.isFile()) {
                seedSize = seedFile.length();
                seedKey = (args.length > 1) ? args[1] : md5Hex(seedFile);
            } else {
                seedSize = 2097152;
                seedKey = (args.length > 1) ? args[1] : "8905e92afeb80fc7722ec89eb0bf0966";
                System.out.println("[WARN] Seed file not found: " + filePath + " (P2P getpieces may return no data)");
            }

            Peer peer = new Peer(autoPort);
            peer.startListening();
            peer.registerFile(
                seedKey,
                filePath,
                seedSize,
                FileState.SEED
            );
            peer.connectToTracker("127.0.0.1", 12345); //we should use the tracker's ip (from args)


            System.out.println("\nPeer initialized successfully!");
            System.out.println("IP: " + peer.getIpAddress());
            System.out.println("Assigned Random Port: " + peer.getPort());
            System.out.println("Seed file: " + filePath);
            System.out.println("Seed key : " + seedKey);

            Scanner scanner = new Scanner(System.in);
            //System.out.println("Type 'echo <port> <hash>' to test leechFile connection (e.g. 'echo 8081 abcdef123').");
            System.out.println("Commands:");
            System.out.println("  look <filename>: search for a file on the tracker");
            System.out.println("  getfile <key>: get peers for a file");
            System.out.println("  interested <port> <key>: ask a peer for its buffermap");
            System.out.println("  getpieces <port> <key> <idx...>: request file pieces from a peer");
            System.out.println("Type 'exit' or 'q' to quit.");
            

            while (true) {
                System.out.print("> ");
                String line = scanner.nextLine().trim();
                if (line.isEmpty()) {
                    continue;
                }

                String[] parts = line.split("\\s+");
                String cmd = parts[0];


                if ("exit".equalsIgnoreCase(cmd) || "q".equalsIgnoreCase(cmd)) {
                    break;
                } else if ("look".equalsIgnoreCase(cmd)) {
                    String filename = line.length() > 5 ? line.substring(5).trim() : "";
                    peer.sendLook(filename);         // to implement next
                } else if ("getfile".equalsIgnoreCase(cmd)) {
                    String key = parts.length > 1 ? parts[1] : "";
                    peer.sendGetFile(key);           // to implement next
                } else if ("interested".equalsIgnoreCase(cmd)) {
                    if (parts.length < 3) {
                        System.out.println("Usage: interested <port> <key>");
                        continue;
                    }

                    int targetPort = Integer.parseInt(parts[1]);
                    String key = parts[2];
                    String response = peer.requestInterested(targetPort, key);
                    System.out.println(response == null ? "No response" : response);
                } else if ("getpieces".equalsIgnoreCase(cmd)) {
                    if (parts.length < 4) {
                        System.out.println("Usage: getpieces <port> <key> <idx...>");
                        continue;
                    }

                    int targetPort = Integer.parseInt(parts[1]);
                    String key = parts[2];
                    List<Integer> indexes = new ArrayList<>();
                    for (int i = 3; i < parts.length; i++) {
                        String token = parts[i].replace("[", "").replace("]", "");
                        if (token.isEmpty()) {
                            continue;
                        }
                        indexes.add(Integer.parseInt(token));
                    }

                    byte[] data = peer.requestPieces(targetPort, key, indexes);
                    if (data == null) {
                        System.out.println("No data received");
                    } else {
                        System.out.println("Received " + data.length + " bytes");
                    }
                } else {
                    System.out.println("Unknown command: " + cmd);
                }
            }

            scanner.close();
            System.out.println("Exiting application...");
            System.exit(0);

        } catch (Exception e) {
            System.err.println("Error initializing Peer application: " + e.getMessage());
        }
    }
}
