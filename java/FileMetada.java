import java.io.File;

public enum FileState {
    SEED,
    LEECH,
    NONE
}

public class FileMetadata {
    private final String MD5hash;
    private final long size;
    private File localPath;
    private String bufferMap; // État des pièce (en base64)
    private FileState state;
    
    public FileMetadata(String MD5hash, String path, long size, FileState state) {
        this.MD5hash = hash;
        this.localPath = new File(path);
        this.size = size;
        this.bufferMap = ""; // À initialiser selon les pièces possédées
        this.state = state;
    }

    public String getFileName() { return localPath.getName(); }
    public String getHash() { return MD5hash; }
    public long getSize() { return size; }
    public File getlocalPath() { return localFile; }
    public void setlocalPath(File file) { this.localPath = file; }
    public void setPath(String newPath) { this.localFile = new File(newPath); }
    public String getBufferMap() { return bufferMap; }
    public void setBufferMap(String bufferMap) { this.bufferMap = bufferMap; }
    public FileState getState() { return state; }
    public void setState(FileState state) { this.state = state; }
}