import java.io.File;

public enum FileState {
    SEED,
    LEECH,
    NONE
}

public class FileMetadata {
    private final String MD5hash;
    private final long size;
    private File FileLocation;
    private String bufferMap; // État des pièce (en base64)
    private FileState state;
    
    public FileMetadata(String MD5hash, String path, long size, FileState state) {
        this.MD5hash = hash;
        this.FileLocation = new File(path);
        this.size = size;
        this.bufferMap = ""; // À initialiser selon les pièces possédées
        this.state = state;
    }

    public String getFileName() { return FileLocation.getName(); }
    public String getHash() { return MD5hash; }
    public long getSize() { return size; }
    public void setSize(long size) { this.size = size; }
    public File getFileLocation() { return localFile; }
    public void setFileLocation(File file) { this.FileLocation = file; }
    public FileState getState() { return state; }
    public void setState(FileState state) { this.state = state; }
}