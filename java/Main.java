import java.net.ServerSocket;
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
            peer.registerFile(
                "8905e92afeb80fc7722ec89eb0bf0966",
                "/tmp/file_a.dat",
                2097152,
                FileState.SEED
            );
            peer.connectToTracker("127.0.0.1", 12345); //we should use the tracker's ip (from args)


            System.out.println("\nPeer initialized successfully!");
            System.out.println("IP: " + peer.getIpAddress());
            System.out.println("Assigned Random Port: " + peer.getPort());

            Scanner scanner = new Scanner(System.in);
            //System.out.println("Type 'echo <port> <hash>' to test leechFile connection (e.g. 'echo 8081 abcdef123').");
            System.out.println("Commands:");
            System.out.println("  look <filename>: search for a file on the tracker");
            System.out.println("  getfile <key>: get peers for a file");
            System.out.println("Type 'exit' or 'q' to quit.");
            

            while (true) {
                System.out.print("> ");
                String line = scanner.nextLine().trim();

                String[] parts = line.split(" ", 2);
                String cmd = parts[0];


                if ("exit".equalsIgnoreCase(cmd) || "q".equalsIgnoreCase(cmd)) {
                    break;
                } else if ("look".equalsIgnoreCase(cmd)) {
                    String filename = parts.length > 1 ? parts[1] : "";
                    peer.sendLook(filename);         // to implement next
                } else if ("getfile".equalsIgnoreCase(cmd)) {
                    String key = parts.length > 1 ? parts[1] : "";
                    peer.sendGetFile(key);           // to implement next
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
