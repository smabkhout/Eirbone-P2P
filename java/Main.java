import java.net.ServerSocket;
import java.util.ArrayList;
import java.util.Base64;
import java.util.List;
import java.util.Scanner;

public class Main {
  public static void main(String[] args) {
    System.out.println("=========================================");
    System.out.println("     Welcome to Eirbone Application      ");
    System.out.println("=========================================");

    try {
      int autoPort;
      ServerSocket s = new ServerSocket(0);
      autoPort = s.getLocalPort();
      s.close();

      Peer peer = new Peer(autoPort);
      peer.startListening();
      peer.connectToTracker(
          "127.0.0.1", 12345); // we should use the tracker's ip (from args)

      try {
        PeerDashboard dashboard = new PeerDashboard(peer);
        int dashPort = dashboard.start();
        System.out.println("Peer dashboard: http://localhost:" + dashPort);
        new ProcessBuilder("xdg-open", "http://localhost:" + dashPort).start();
      } catch (Exception e) {
        System.err.println("[WARN] Could not start peer dashboard: " + e.getMessage());
      }

      System.out.println("\nPeer initialized successfully!");
      System.out.println("IP: " + peer.getIpAddress());
      System.out.println("Assigned Random Port: " + peer.getPort());
      System.out.println("Initial files: 0");

      Scanner scanner = new Scanner(System.in);
      // System.out.println("Type 'echo <port> <hash>' to test leechFile
      // connection (e.g. 'echo 8081 abcdef123').");
      System.out.println("Commands:");
      System.out.println("  look <filename>: search for a file on the tracker");
      System.out.println("  getfile <key>: get peers for a file");
      System.out.println(
          "  interested <port> <key>: ask a peer for its buffermap");
      System.out.println(
          "  getpieces <port> <key> <idx...>: request file pieces from a peer");
        System.out.println(
          "  getpieces <port> <key> [:]    : request all available pieces from a peer");
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
          String res = peer.lookRaw(filename);
          System.out.println(res == null ? "No files found." : res);
        } else if ("getfile".equalsIgnoreCase(cmd)) {
          String key = parts.length > 1 ? parts[1] : "";
          peer.sendGetFile(key); // to implement next
        } else if ("interested".equalsIgnoreCase(cmd)) {
          if (parts.length < 3) {
            System.out.println("Usage: interested <port> <key>");
            continue;
          }

                    int targetPort = Integer.parseInt(parts[1]);
                    String key = parts[2];
                    String response = peer.requestInterested(targetPort, key);
                    if (response == null) {
                        System.out.println("No response");
                    } else {
                        System.out.println(response);
                        System.out.println("(pieces disponibles: " + decodeBuffermapToIndexes(response) + ")");
                    }
                } else if ("getpieces".equalsIgnoreCase(cmd)) {
                    if (parts.length < 4) {
                        System.out.println("Usage: getpieces <port> <key> <idx...>|[:]");
                        continue;
                    }

          int targetPort = Integer.parseInt(parts[1]);
          String key = parts[2];
          List<Integer> indexes = new ArrayList<>();
          boolean allShortcut = parts.length == 4 &&
              ("[:]".equals(parts[3]) || ":".equals(parts[3]) || "all".equalsIgnoreCase(parts[3]));

          if (allShortcut) {
            String have = peer.requestInterested(targetPort, key);
            indexes = decodeBuffermapResponseToIndexes(have);
            if (indexes.isEmpty()) {
              System.out.println("No available piece for this file");
              continue;
            }
          } else {
            for (int i = 3; i < parts.length; i++) {
              String token = parts[i].replace("[", "").replace("]", "");
              if (token.isEmpty()) {
                continue;
              }
              indexes.add(Integer.parseInt(token));
            }
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

    private static String decodeBuffermapToIndexes(String response) {
      List<Integer> indexes = decodeBuffermapResponseToIndexes(response);
      if (indexes.isEmpty()) {
        return "[]";
      }

      StringBuilder sb = new StringBuilder("[");
      for (int i = 0; i < indexes.size(); i++) {
        if (i > 0) {
          sb.append(" ");
        }
        sb.append(indexes.get(i));
      }
      sb.append("]");
      return sb.toString();
    }

    private static List<Integer> decodeBuffermapResponseToIndexes(String response) {
      List<Integer> indexes = new ArrayList<>();
      if (response == null || response.trim().isEmpty()) {
        return indexes;
      }
        String[] tokens = response.trim().split("\\s+", 3);
        if (tokens.length < 3 || !"have".equals(tokens[0])) {
        return indexes;
        }

        try {
            byte[] buffermap = Base64.getDecoder().decode(tokens[2]);

            for (int byteIndex = 0; byteIndex < buffermap.length; byteIndex++) {
                for (int bit = 0; bit < 8; bit++) {
                    int mask = 1 << (7 - bit);
                    if ((buffermap[byteIndex] & mask) != 0) {
                        indexes.add(byteIndex * 8 + bit);
                    }
                }
            }
        } catch (IllegalArgumentException e) {
                return new ArrayList<>();
        }

              return indexes;
    }
}
