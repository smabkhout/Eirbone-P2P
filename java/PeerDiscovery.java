import java.util.ArrayList;
import java.util.List;

public class PeerDiscovery {
  private String fileHash;
  private List<RemotePeer> potentialSeeds;

  public PeerDiscovery(String hash) {
    this.fileHash = hash;
    this.potentialSeeds = new ArrayList<>();
  }

  // Method to parse the Tracker's response: "peers <hash>
  // [1.1.1.2:2222 1.1.1.3:3333]"
  public void updatePeersFromTracker(String response) {
    // 1. Remove brackets and prefix
    String listPart =
        response.substring(response.indexOf("[") + 1, response.indexOf("]"));

    // 2. Split by space
    String[] peerStrings = listPart.split(" ");

    for (String p : peerStrings) {
      if (!p.isEmpty()) {
        String[] parts = p.split(":");
        String ip = parts[0];
        int port = Integer.parseInt(parts[1]);

        // Add to our list of people to contact
        potentialSeeds.add(new RemotePeer(ip, port));
      }
    }
  }
}

// Simple helper class for neighbors
class RemotePeer {
  String ip;
  int port;
  public RemotePeer(String ip, int port) {
    this.ip = ip;
    this.port = port;
  }
}