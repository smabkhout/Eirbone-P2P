import java.io.*;
import java.net.*;
import java.util.HashMap;
import java.util.Map;

public class Peer {
    private InetAddress ip;
    private int port;
    
    // Clé = Hash MD5 du fichier, Valeur = Détails (chemin, taille, pièces...)
    private Map<String, FileMetadata> managedFiles;

    public Peer(InetAddress ip, int port) {
        this.ip = ip;
        this.port = port;
        this.managedFiles = new HashMap<>();
    }

    public void registerFile(String MD5hash, String path, long size) {
        FileMetadata fm = new FileMetadata(MD5hash, path, size);
        this.managedFiles.put(MD5hash, fm);
    }

    public String getIpAddress() { return ip.getHostAddress(); }
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

    
}




























public class Peer 
{
    x

	static final int port = 8080;
	public static void main (String[] args) throws Exception {
	
	Socket socket = new Socket (args [0], port) ;
	System.out.println ("SOCKET = " + socket);
	BufferedReader plec = new BufferedReader (
	new InputStreamReader (socket.getInputStream () )
	);
	
	PrintWriter pred = new PrintWriter (
	new BufferedWriter (
	new OutputStreamWriter (socket.getOutputStream () ) ),
	true);
	
	String str = "bonjour";
	
	for (int i = 0; i < 10; i++) {
		pred.println (str);
		str = plec.readLine();
	}
	
	System.out.println ("END");import java.io.*;
import java.net.*;

public class Client 
{
	static final int port = 8080;
	public static void main (String[] args) throws Exception {
	
	Socket socket = new Socket (args [0], port) ;
	System.out.println ("SOCKET = " + socket);
	BufferedReader plec = new BufferedReader (
	new InputStreamReader (socket.getInputStream () )
	);
	
	PrintWriter pred = new PrintWriter (
	new BufferedWriter (
	new OutputStreamWriter (socket.getOutputStream () ) ),
	true);
	
	String str = "bonjour";
	
	for (int i = 0; i < 10; i++) {
		pred.println (str);
		str = plec.readLine();
	}
	
	System.out.println ("END");
	pred. println ("END");
	plec.close ();
	pred.close();
	socket.close ();
	}
}
	pred. println ("END");
	plec.close ();
	pred.close();
	socket.close ();
	}
}