import java.io.File;
import java.io.FileOutputStream;
import java.net.ServerSocket;
import java.util.Arrays;
import java.util.Base64;

public class PeerProtocolTest {
    public static void main(String[] args) throws Exception {
        System.out.println("Testing Peer-to-Peer protocol...\n");

        testInterestedHaveBetweenTwoPeers();
        testGetPiecesDataBetweenTwoPeers();
        testReceivedPiecesAreRecordedAndAdvertised();

        System.out.println("\nOK");
    }

    private static void testInterestedHaveBetweenTwoPeers() throws Exception {
        System.out.println("TEST interested/have between two peers");

        int serverPort = getFreePort();
        int clientPort = getFreePort();

        Peer serverPeer = new Peer(serverPort);
        Peer clientPeer = new Peer(clientPort);

        File tempFile = createTempFileWithPattern(2048);
        String key = "8905e92afeb80fc7722ec89eb0bf0966";

        serverPeer.registerFile(key, tempFile.getAbsolutePath(), tempFile.length(), FileState.SEED);
        serverPeer.startListening();
        clientPeer.startListening();

        String response = clientPeer.requestInterested(serverPort, key);
        assertTrue(response != null, "No HAVE response received");
        assertTrue(response.startsWith("have " + key + " "), "Unexpected have response: " + response);

        String[] parts = response.split(" ", 3);
        assertTrue(parts.length == 3, "Malformed have response: " + response);

        byte[] decoded = Base64.getDecoder().decode(parts[2].trim());
        assertTrue(decoded.length == 1, "Expected 1-byte buffermap, got: " + decoded.length);
        assertTrue((decoded[0] & 0xFF) == 0xC0, "Unexpected buffermap value: 0x" + Integer.toHexString(decoded[0] & 0xFF));

        tempFile.delete();
        System.out.println("PASSED");
    }

    private static void testGetPiecesDataBetweenTwoPeers() throws Exception {
        System.out.println("TEST getpieces/data between two peers");

        int serverPort = getFreePort();
        int clientPort = getFreePort();

        Peer serverPeer = new Peer(serverPort);
        Peer clientPeer = new Peer(clientPort);

        File tempFile = createTempFileWithPattern(3072);
        String key = "330a57722ec8b0bf09669a2b35f88e9e";

        serverPeer.registerFile(key, tempFile.getAbsolutePath(), tempFile.length(), FileState.SEED);
        serverPeer.startListening();
        Thread.sleep(200);

        String have = clientPeer.requestInterested(serverPort, key);
        assertTrue(have != null && have.startsWith("have " + key + " "), "Unexpected HAVE response: " + have);

        byte[] payload = clientPeer.requestPieces(serverPort, key, Arrays.asList(0, 1));
        assertTrue(payload != null, "No DATA payload received");
        assertTrue(payload.length == 2048, "Payload length mismatch: " + payload.length);

        for (int i = 0; i < payload.length; i++) {
            byte expected = (byte) (i % 256);
            assertTrue(payload[i] == expected, "Unexpected payload byte at index " + i);
        }

        tempFile.delete();
        System.out.println("PASSED");
    }

    private static void testReceivedPiecesAreRecordedAndAdvertised() throws Exception {
        System.out.println("TEST received pieces are recorded");

        int serverPort = getFreePort();
        int clientPort = getFreePort();

        Peer serverPeer = new Peer(serverPort);
        Peer clientPeer = new Peer(clientPort);

        File tempFile = createTempFileWithPattern(2048);
        String key = "8905e92afeb80fc7722ec89eb0bf0966";

        serverPeer.registerFile(key, tempFile.getAbsolutePath(), tempFile.length(), FileState.SEED);
        serverPeer.startListening();
        Thread.sleep(200);

        byte[] payload = clientPeer.requestPieces(serverPort, key, Arrays.asList(0));
        assertTrue(payload != null, "No payload received");

        FileMetadata fm = clientPeer.getManagedFiles().get(key);
        assertTrue(fm != null, "Received pieces were not recorded locally");
        assertTrue(fm.getState() == FileState.LEECH, "Expected leech state after receiving pieces");
        assertTrue(fm.getBufferMap() != null && !fm.getBufferMap().isEmpty(), "Expected a non-empty buffermap after recording pieces");
        assertTrue(fm.getlocalPath() != null && fm.getlocalPath().exists(), "Expected received pieces to be written to disk");

        String update = clientPeer.buildUpdateRequest();
        assertTrue(update.contains("leech [" + key + "]") || update.contains("leech [" + key + " "), "Update request should advertise the received key: " + update);

        tempFile.delete();
        System.out.println("PASSED");
    }

    private static int getFreePort() throws Exception {
        try (ServerSocket socket = new ServerSocket(0)) {
            return socket.getLocalPort();
        }
    }

    private static File createTempFileWithPattern(int size) throws Exception {
        File file = File.createTempFile("eirbone-test-", ".bin");
        try (FileOutputStream out = new FileOutputStream(file)) {
            for (int i = 0; i < size; i++) {
                out.write(i % 256);
            }
        }
        file.deleteOnExit();
        return file;
    }

    private static void assertTrue(boolean condition, String message) {
        if (!condition) {
            throw new RuntimeException(message);
        }
    }
}