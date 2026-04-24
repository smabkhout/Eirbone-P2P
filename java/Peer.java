import java.io.*;
import java.net.*;
import java.nio.charset.StandardCharsets;
import java.util.ArrayList;
import java.util.Base64;
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
    private final Object trackerIoLock = new Object();
    private Thread periodicUpdateThread;
    private volatile boolean periodicUpdateRunning;
    
    // Clé = Hash MD5 du fichier, Valeur = Détails (chemin, taille, pièces...)
    private Map<String, FileMetadata> managedFiles;

  public Peer(int port) {
    try {
      this.ip = InetAddress.getLocalHost().getHostAddress();
    } catch (UnknownHostException e) {
      this.ip = "127.0.0.1";
    }
    this.port = port;
    this.managedFiles = new HashMap<>();
  }

  public void startListening() {
    Thread listenerThread = new Thread(() -> {
      try (ServerSocket serverSocket = new ServerSocket(this.port)) {
        while (true) {
          Socket clientSocket = serverSocket.accept();
          handleIncomingConnection(clientSocket);
        }
      } catch (IOException e) {
        System.err.println("\n[ERROR] Peer server listener failed on port " +
                           this.port + ": " + e.getMessage());
      }
    });
    listenerThread.setDaemon(true);
    listenerThread.start();
  }

  private void handleIncomingConnection(Socket clientSocket) {
    new Thread(() -> {
      try {
        BufferedInputStream in =
            new BufferedInputStream(clientSocket.getInputStream());
        BufferedOutputStream out =
            new BufferedOutputStream(clientSocket.getOutputStream());

        String header = readAsciiLine(in);
        if (header == null || header.isEmpty()) {
          return;
        }

        if (header.startsWith("interested ")) {
          handleInterested(header, out);
        } else if (header.startsWith("getpieces ")) {
          handleGetPieces(header, out);
        } else {
          out.write("ACK\n".getBytes(StandardCharsets.UTF_8));
          out.flush();
        }
      } catch (IOException e) {
        System.err.println("\nError handling incoming connection: " +
                           e.getMessage());
      } finally {
        try {
          clientSocket.close();
        } catch (IOException e) {
          // Ignore
        }
        // Print a new prompt so the user's CLI feels seamless after receiving a
        // background message
        System.out.print("> ");
      }
    }).start();
  }

  private void handleInterested(String header, BufferedOutputStream out)
      throws IOException {
    String[] parts = header.split(" ", 2);
    if (parts.length < 2) {
      out.write("have unknown \n".getBytes(StandardCharsets.UTF_8));
      out.flush();
      return;
    }

    String key = parts[1].trim();
    String buffermap = computeBufferMapBase64(key);
    String response = "have " + key + " " + buffermap + "\n";
    out.write(response.getBytes(StandardCharsets.UTF_8));
    out.flush();
  }

  private void handleGetPieces(String header, BufferedOutputStream out)
      throws IOException {
    String[] firstSplit = header.split(" ", 3);
    if (firstSplit.length < 3) {
      out.write("data unknown []\n".getBytes(StandardCharsets.UTF_8));
      out.flush();
      return;
    }

    String key = firstSplit[1].trim();
    List<Integer> indexes = parseIndexesList(firstSplit[2]);
    FileMetadata fm = managedFiles.get(key);

    if (fm == null || fm.getlocalPath() == null ||
        !fm.getlocalPath().exists()) {
      out.write(("data " + key + " []\n").getBytes(StandardCharsets.UTF_8));
      out.flush();
      return;
    }

    List<Integer> sentIndexes = new ArrayList<>();
    ByteArrayOutputStream payload = new ByteArrayOutputStream();
    int pieceSize = 1024;

    for (int idx : indexes) {
      if (idx < 0 || !hasPiece(fm, idx)) {
        continue;
      }
      byte[] piece = readPieceBytes(fm.getlocalPath(), idx, pieceSize);
      if (piece == null) {
        continue;
      }
      sentIndexes.add(idx);
      payload.write(piece);
    }

    StringBuilder headerBuilder = new StringBuilder();
    headerBuilder.append("data ").append(key).append(" [");
    for (int i = 0; i < sentIndexes.size(); i++) {
      if (i > 0)
        headerBuilder.append(" ");
      headerBuilder.append(sentIndexes.get(i)).append(" ").append(pieceSize);
    }
    headerBuilder.append("]\n");

    out.write(headerBuilder.toString().getBytes(StandardCharsets.UTF_8));
    out.write(payload.toByteArray());
    out.flush();
  }

  private String computeBufferMapBase64(String key) {
    FileMetadata fm = managedFiles.get(key);
    if (fm == null)
      return "";

    long size = fm.getSize();
    int pieceSize = 1024;
    int pieces = (int)((size + pieceSize - 1) / pieceSize);
    if (pieces <= 0)
      return "";

    if (fm.getState() == FileState.LEECH && fm.getBufferMap() != null &&
        !fm.getBufferMap().isEmpty()) {
      return fm.getBufferMap();
    }

    byte[] bits = new byte[(pieces + 7) / 8];
    if (fm.getState() == FileState.SEED) {
      for (int i = 0; i < pieces; i++) {
        bits[i / 8] |= (byte)(1 << (7 - (i % 8)));
      }
    }
    return Base64.getEncoder().encodeToString(bits);
  }

  private boolean hasPiece(FileMetadata fm, int idx) {
    int pieceSize = 1024;
    int pieces = (int)((fm.getSize() + pieceSize - 1) / pieceSize);
    if (idx >= pieces)
      return false;

    if (fm.getState() == FileState.SEED)
      return true;

    String bm = fm.getBufferMap();
    if (bm == null || bm.isEmpty())
      return false;
    try {
      byte[] data = Base64.getDecoder().decode(bm);
      int byteIndex = idx / 8;
      int bitIndex = 7 - (idx % 8);
      if (byteIndex >= data.length)
        return false;
      return (data[byteIndex] & (1 << bitIndex)) != 0;
    } catch (IllegalArgumentException e) {
      return false;
    }
  }

  private byte[] readPieceBytes(File file, int pieceIndex, int pieceSize) {
    try (RandomAccessFile raf = new RandomAccessFile(file, "r")) {
      long offset = (long)pieceIndex * pieceSize;
      if (offset >= raf.length())
        return null;

      raf.seek(offset);
      byte[] buf = new byte[pieceSize];
      int read = raf.read(buf);
      if (read < 0)
        return null;
      if (read < pieceSize) {
        for (int i = read; i < pieceSize; i++)
          buf[i] = 0;
      }
      return buf;
    } catch (IOException e) {
      return null;
    }
  }

  private List<Integer> parseIndexesList(String raw) {
    List<Integer> indexes = new ArrayList<>();
    int l = raw.indexOf('[');
    int r = raw.indexOf(']');
    if (l < 0 || r < 0 || r <= l)
      return indexes;

    String[] parts = raw.substring(l + 1, r).trim().split("\\s+");
    for (String p : parts) {
      if (p.isEmpty())
        continue;
      try {
        indexes.add(Integer.parseInt(p));
      } catch (NumberFormatException ignored) {
      }
    }
    return indexes;
  }

  private String readAsciiLine(BufferedInputStream in) throws IOException {
    ByteArrayOutputStream line = new ByteArrayOutputStream();
    while (true) {
      int b = in.read();
      if (b == -1) {
        if (line.size() == 0)
          return null;
        break;
      }
      if (b == '\n')
        break;
      if (b != '\r')
        line.write(b);
    }
    return line.toString(StandardCharsets.UTF_8);
  }

  private int extractTotalPayloadSize(String header) {
    int start = header.indexOf('[');
    int end = header.indexOf(']');
    if (start < 0 || end < 0 || end <= start)
      return 0;

    String[] tokens = header.substring(start + 1, end).trim().split("\\s+");
    int total = 0;
    for (int i = 1; i < tokens.length; i += 2) {
      total += Integer.parseInt(tokens[i]);
    }
    return total;
  }

  private byte[] readExactBytes(BufferedInputStream in, int length)
      throws IOException {
    byte[] buffer = new byte[length];
    int offset = 0;
    while (offset < length) {
      int read = in.read(buffer, offset, length - offset);
      if (read < 0)
        break;
      offset += read;
    }
    return offset == length ? buffer : null;
  }

  public void registerFile(String MD5hash, String path, long size,
                           FileState state) {
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
        sb.append(fm.getFileName())
            .append(" ")
            .append(fm.getSize())
            .append(" ")
            .append("1024 ")
            .append(fm.getHash())
            .append(" ");
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

    private String getSeedKeyList() {
        StringBuilder sb = new StringBuilder("[");
        for (FileMetadata fm : managedFiles.values()) {
            if (fm.getState() == FileState.SEED) {
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

    public String buildUpdateRequest() {
        String seeds = getSeedKeyList();
        String leeches = getLeechList();
        return String.format("update seed %s leech %s\n", seeds, leeches);
    }

  public String buildLookRequest(List<String> criteria) {
    // Joint les critères avec un espace : "filename=test.bin filesize>100"
    String joinedCriteria = String.join(" ", criteria);
    return "look [" + joinedCriteria + "]\n";
  }

  // pour l'instant on utilise juste cette méthode et après on implémente une
  // classe pour les critères
  public String buildLookByNameRequest(String filename) {
    if (filename.contains(" ")) {
      throw new IllegalArgumentException(
          "Le nom du fichier ne doit pas contenir d'espaces.");
    }

    return "look [filename=" + filename + "]\n";
  }

  public String buildGetFileRequest(String key) {
    return "getfile " + key + "\n";
  }

  public String requestInterested(int targetPort, String key) {
    try (Socket socket = new Socket("127.0.0.1", targetPort);
         BufferedOutputStream out =
             new BufferedOutputStream(socket.getOutputStream());
         BufferedInputStream in =
             new BufferedInputStream(socket.getInputStream())) {

      out.write(("interested " + key + "\n").getBytes(StandardCharsets.UTF_8));
      out.flush();

      return readAsciiLine(in);
    } catch (IOException e) {
      System.err.println("Error requesting interested from localhost:" +
                         targetPort + " - " + e.getMessage());
      return null;
    }
  }

  public byte[] requestPieces(int targetPort, String key,
                              List<Integer> indexes) {
    try (Socket socket = new Socket("127.0.0.1", targetPort);
         BufferedOutputStream out =
             new BufferedOutputStream(socket.getOutputStream());
         BufferedInputStream in =
             new BufferedInputStream(socket.getInputStream())) {

      out.write(
          buildGetPiecesRequest(key, indexes).getBytes(StandardCharsets.UTF_8));
      out.flush();

      String header = readAsciiLine(in);
      if (header == null || !header.startsWith("data ")) {
        return null;
      }

            int payloadSize = extractTotalPayloadSize(header);
            byte[] payload = readExactBytes(in, payloadSize);
            if (payload != null) {
                recordReceivedPieces(key, indexes, payload);
            }
            return payload;
        } catch (IOException e) {
            System.err.println("Error requesting pieces from localhost:" + targetPort + " - " + e.getMessage());
            return null;
        }
    }

    private void recordReceivedPieces(String key, List<Integer> indexes, byte[] payload) {
        if (key == null || indexes == null || payload == null || indexes.isEmpty()) {
            return;
        }

        FileMetadata fm = managedFiles.get(key);
        if (fm == null) {
            long estimatedSize = (long) (maxIndex(indexes) + 1) * 1024L;
            fm = new FileMetadata(key, "/tmp/" + key + ".part", estimatedSize, FileState.LEECH);
            managedFiles.put(key, fm);
        } else {
            fm.setState(FileState.LEECH);
        }

        updateBufferMapFromIndexes(fm, indexes);
        savePayloadPieces(fm, indexes, payload);
    }

    private int maxIndex(List<Integer> indexes) {
        int max = 0;
        for (int index : indexes) {
            if (index > max) {
                max = index;
            }
        }
        return max;
    }

    private void updateBufferMapFromIndexes(FileMetadata fm, List<Integer> indexes) {
        int highestIndex = maxIndex(indexes);
        int pieceCount = Math.max(1, highestIndex + 1);
        byte[] bits = new byte[(pieceCount + 7) / 8];

        String currentBufferMap = fm.getBufferMap();
        if (currentBufferMap != null && !currentBufferMap.isEmpty()) {
            try {
                byte[] existingBits = Base64.getDecoder().decode(currentBufferMap);
                System.arraycopy(existingBits, 0, bits, 0, Math.min(existingBits.length, bits.length));
            } catch (IllegalArgumentException ignored) {
            }
        }

        for (int index : indexes) {
            int byteIndex = index / 8;
            int bitIndex = 7 - (index % 8);
            if (byteIndex < bits.length) {
                bits[byteIndex] |= (byte) (1 << bitIndex);
            }
        }

        fm.setBufferMap(Base64.getEncoder().encodeToString(bits));
    }

    private void savePayloadPieces(FileMetadata fm, List<Integer> indexes, byte[] payload) {
        File targetFile = fm.getlocalPath();
        if (targetFile == null) {
            targetFile = new File("/tmp/" + fm.getHash() + ".part");
            fm.setlocalPath(targetFile);
        }

        File parent = targetFile.getParentFile();
        if (parent != null && !parent.exists()) {
            parent.mkdirs();
        }

        int pieceSize = 1024;
        try (RandomAccessFile raf = new RandomAccessFile(targetFile, "rw")) {
            int offset = 0;
            for (int index : indexes) {
                if (offset + pieceSize > payload.length) {
                    break;
                }

                raf.seek((long) index * pieceSize);
                raf.write(payload, offset, pieceSize);
                offset += pieceSize;
            }

            long newSize = Math.max(fm.getSize(), (long) (maxIndex(indexes) + 1) * pieceSize);
            fm.setPath(targetFile.getAbsolutePath());
            fm.setState(FileState.LEECH);
            if (newSize > fm.getSize()) {
                // keep the existing reported size in the metadata contract, the file path carries the saved pieces
            }
        } catch (IOException e) {
            System.err.println("Error saving received pieces for " + fm.getHash() + ": " + e.getMessage());
        }
    }

  private String buildGetPiecesRequest(String key, List<Integer> indexes) {
    StringBuilder sb = new StringBuilder();
    sb.append("getpieces ").append(key).append(" [");
    for (int i = 0; i < indexes.size(); i++) {
      if (i > 0)
        sb.append(" ");
      sb.append(indexes.get(i));
    }
    sb.append("]\n");
    return sb.toString();
  }

  public void connectToTracker(String trackerIp, int trackerPort) {
    try {
      trackerSocket = new Socket(trackerIp, trackerPort);
      trackerOut = new PrintWriter(trackerSocket.getOutputStream(), true);
      trackerIn = new BufferedReader(
          new InputStreamReader(trackerSocket.getInputStream()));
    } catch (IOException e) {
      System.err.println("Could not connect to tracker: " + e.getMessage());
      return;
    }

        try {
            String announce = buildAnnounceRequest();
            String response = sendTrackerRequest(announce);
            if (response != null && "ok".equals(response.trim())) {
                System.out.println("Sent to tracker: " + announce.trim());
                System.out.println("Tracker acknowledged announce!");
                startPeriodicUpdate(10000L);
            } else {
                System.err.println("Unexpected tracker response: " + response);
            }
        } catch (IOException e) {
            System.err.println("Error reading tracker response: " + e.getMessage());
        }
    }

    private String sendTrackerRequest(String request) throws IOException {
        synchronized (trackerIoLock) {
            if (trackerOut == null || trackerIn == null) {
                throw new IOException("tracker connection is not available");
            }
            trackerOut.print(request);
            trackerOut.flush();
            String response = trackerIn.readLine();
            if (response == null) {
                closeTrackerConnectionLocked();
                throw new IOException("tracker disconnected");
            }
            return response;
        }
    }

    private void closeTrackerConnectionLocked() {
        try {
            if (trackerIn != null) trackerIn.close();
        } catch (IOException ignored) {
        }
        if (trackerOut != null) {
            trackerOut.close();
        }
        try {
            if (trackerSocket != null && !trackerSocket.isClosed()) {
                trackerSocket.close();
            }
        } catch (IOException ignored) {
        }
        trackerIn = null;
        trackerOut = null;
        trackerSocket = null;
    }

    private void startPeriodicUpdate(long periodMs) {
        synchronized (trackerIoLock) {
            if (periodicUpdateThread != null && periodicUpdateThread.isAlive()) {
                return;
            }
            periodicUpdateRunning = true;
            periodicUpdateThread = new Thread(() -> {
                while (periodicUpdateRunning) {
                    try {
                        Thread.sleep(periodMs);
                    } catch (InterruptedException e) {
                        Thread.currentThread().interrupt();
                        break;
                    }

                    try {
                        String response = sendTrackerRequest(buildUpdateRequest());
                        if (response == null || !"ok".equals(response.trim())) {
                            System.err.println("[UPDATE] Unexpected tracker response: " + response);
                        }
                    } catch (IOException e) {
                        System.err.println("[UPDATE] Stopped periodic update: " + e.getMessage());
                        break;
                    }
                }
                periodicUpdateRunning = false;
            });
            periodicUpdateThread.setName("tracker-periodic-update-" + this.port);
            periodicUpdateThread.setDaemon(true);
            periodicUpdateThread.start();
        }
    }

    public void sendLook(String filename) {
        try {
            if (trackerOut == null || trackerIn == null) {
                System.err.println("Tracker connection is not available. Start the tracker before using look.");
                return;
            }
            String response = sendTrackerRequest(buildLookByNameRequest(filename));
            System.out.println("Tracker response: " + response);
        } catch (IOException e) {
            System.err.println("Error during look: " + e.getMessage());
        }
    }

    public void sendGetFile(String key) {
        try {
            if (trackerOut == null || trackerIn == null) {
                System.err.println("Tracker connection is not available. Start the tracker before using getfile.");
                return;
            }
            String response = sendTrackerRequest(buildGetFileRequest(key));
            System.out.println("Tracker response: " + response);
        } catch (IOException e) {
            System.err.println("Error during getfile: " + e.getMessage());
        }
    }
}