import java.io.*;
import java.net.*;
import java.util.HashMap;
import java.util.List;
import java.util.Map;

public class Peer {
    private String ip;
    private int port;

    //for the connection
    private Socket trackerSocket;
    private PrintWriter trackerOut;
    private BufferedReader trackerIn;
    
    // Clé = Hash MD5 du fichier, Valeur = Détails (chemin, taille, pièces...)
    private Map<String, FileMetadata> managedFiles;

    public Peer(int port) {
        this.ip = "127.0.0.1";
        this.port = port;
        this.managedFiles = new HashMap<>();
    }

    public void startListening() {
        new Thread(() -> {
            try (ServerSocket serverSocket = new ServerSocket(this.port)) {
                while (true) {
                    Socket clientSocket = serverSocket.accept();
                    handleIncomingConnection(clientSocket);
                }
            } catch (IOException e) {
                System.err.println("\n[ERROR] Peer server listener failed on port " + this.port + ": " + e.getMessage());
            }
        }).start();
    }

    private void handleIncomingConnection(Socket clientSocket) {
        new Thread(() -> {
            try (
                BufferedReader in = new BufferedReader(new InputStreamReader(clientSocket.getInputStream()));
                PrintWriter out = new PrintWriter(clientSocket.getOutputStream(), true)
            ) {
                String inputLine = in.readLine();
                if (inputLine != null) {
                    if (inputLine.startsWith("ECHO")) {
                        String[] parts = inputLine.split(" ", 3);
                        if (parts.length >= 3) {
                            String senderPort = parts[1];
                            String hash = parts[2];
                            System.out.println("\n[P2P MESSAGE] ECHO received from Peer explicitly listening on port " + senderPort + " | Requested hash: " + hash);
                            out.println("ECHO_REPLY for " + hash);
                        } else {
                            System.out.println("\n[P2P MESSAGE] Received malformed ECHO: " + inputLine);
                            out.println("ECHO_REPLY (malformed)");
                        }
                    } else {
                        System.out.println("\n[P2P MESSAGE] -> " + inputLine);
                        out.println("ACK");
                    }
                }
            } catch (IOException e) {
                System.err.println("\nError handling incoming connection: " + e.getMessage());
            } finally {
                try {
                    clientSocket.close();
                } catch (IOException e) {
                    // Ignore
                }
                // Print a new prompt so the user's CLI feels seamless after receiving a background message
                System.out.print("> ");
            }
        }).start();
    }

    public void registerFile(String MD5hash, String path, long size, FileState state) {
        FileMetadata fm = new FileMetadata(MD5hash, path, size);
        fm.setState(state);
        this.managedFiles.put(MD5hash, fm);
    }

    public String getIpAddress() { return ip; }
    public int getPort() { return port; }
    public Map<String, FileMetadata> getManagedFiles() { return managedFiles; }

    private String getSeedList() {
        StringBuilder sb = new StringBuilder("[");
        for (FileMetadata fm : managedFiles.values()) {
            if (fm.getState() == FileState.SEED) {
                sb.append(fm.getFileName()).append(" ")
                  .append(fm.getSize()).append(" ")
                  .append("1024 ")
                  .append(fm.getHash()).append(" ");
            }
        }
        return sb.toString().trim() + "]";
    }

    private String getLeechList() {
        StringBuilder sb = new StringBuilder("[");
        for (FileMetadata fm : managedFiles.values()) {
            if (fm.getState() == FileState.LEECH) {
                sb.append(fm.getHash()).append(" ");
            }
        }
        return sb.toString().trim() + "]";
    }

    public String buildAnnounceRequest() {
        String seeds = getSeedList();
        String leeches = getLeechList();
        return String.format("announce listen %d seed %s leech %s\n", 
                             this.port, seeds, leeches);
    }

    public String buildLookRequest(List<String> criteria) {
        // Joint les critères avec un espace : "filename=test.bin filesize>100"
        String joinedCriteria = String.join(" ", criteria);
        return "look [" + joinedCriteria + "]\n";
    }

    // pour l'instant on utilise juste cette méthode et après on implémente une classe pour les critères
    public String buildLookByNameRequest(String filename) {
        if (filename.contains(" ")) {
            throw new IllegalArgumentException("Le nom du fichier ne doit pas contenir d'espaces.");
        }
        
        return "look [filename=" + filename + "]\n";
    }

    public String buildGetFileRequest(String key) {
        return "getfile " + key + "\n";
    }

    // envoie juste un echo pour l'instant pour verifie si les connexions marchent bien
    public void leechFile(int targetPort, String MD5hash) {
        try (Socket socket = new Socket("localhost", targetPort);
             PrintWriter out = new PrintWriter(socket.getOutputStream(), true);
             BufferedReader in = new BufferedReader(new InputStreamReader(socket.getInputStream()))) {
            
            String echoMessage = "ECHO " + this.port + " " + MD5hash;
            out.println(echoMessage);
            System.out.println("Sent to localhost:" + targetPort + " -> " + echoMessage);
            
            String response = in.readLine();
            System.out.println("Received from localhost:" + targetPort + " <- " + response);
            
        } catch (IOException e) {
            System.err.println("Error connecting to localhost:" + targetPort + " - " + e.getMessage());
        }
    }

    public void connectToTracker(String trackerIp, int trackerPort) {
        try {
            trackerSocket = new Socket(trackerIp, trackerPort);
            trackerOut = new PrintWriter(trackerSocket.getOutputStream(), true);
            trackerIn  = new BufferedReader(new InputStreamReader(trackerSocket.getInputStream()));
        } catch (IOException e) {
            System.err.println("Could not connect to tracker: " + e.getMessage());
            return;
        }

        String announce = buildAnnounceRequest();
        trackerOut.print(announce);
        trackerOut.flush();
        System.out.println("Sent to tracker: " + announce);

        try {
            String response = trackerIn.readLine();
            if ("ok".equals(response.trim())) {
                System.out.println("Tracker acknowledged announce!");
            } else {
                System.err.println("Unexpected tracker response: " + response);
            }
        } catch (IOException e) {
            System.err.println("Error reading tracker response: " + e.getMessage());
        }
    }

    public void sendLook(String filename) {
        try {
            trackerOut.print(buildLookByNameRequest(filename));
            trackerOut.flush();
            String response = trackerIn.readLine();
            System.out.println("Tracker response: " + response);
        } catch (IOException e) {
            System.err.println("Error during look: " + e.getMessage());
        }
    }

    public void sendGetFile(String key) {
        try {
            trackerOut.print(buildGetFileRequest(key));
            trackerOut.flush();
            String response = trackerIn.readLine();
            System.out.println("Tracker response: " + response);
        } catch (IOException e) {
            System.err.println("Error during getfile: " + e.getMessage());
        }
    }
}