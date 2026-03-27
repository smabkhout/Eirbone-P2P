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

            System.out.println("\nPeer initialized successfully!");
            System.out.println("IP: " + peer.getIpAddress());
            System.out.println("Assigned Random Port: " + peer.getPort());
            System.out.println("Ready to connect to other peers.");
            System.out.println("=========================================");

            Scanner scanner = new Scanner(System.in);
            System.out.println("Type 'echo <port> <hash>' to test leechFile connection (e.g. 'echo 8081 abcdef123').");
            System.out.println("Type 'exit' or 'q' to quit.");

            while (true) {
                System.out.print("> ");
                String input = scanner.next();

                if ("exit".equalsIgnoreCase(input) || "q".equalsIgnoreCase(input)) {
                    break;
                } else if ("echo".equalsIgnoreCase(input)) { // il faut qu on implemente le parser des commandes ici
                                                             // après (announce, listen, leech, ...etc)
                    if (scanner.hasNextInt()) {
                        int targetPort = scanner.nextInt();
                        String hash = scanner.next();
                        peer.leechFile(targetPort, hash);
                    } else {
                        System.out.println("Invalid port number.");
                        scanner.nextLine();
                    }
                } else {
                    System.out.println("Unknown command: " + input);
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
