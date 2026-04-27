import java.io.File;

enum FileState { SEED, LEECH, NONE }

public class FileMetadata {
  private final String MD5hash;
  private long size;
  private File localPath;
  private String bufferMap;
  private FileState state;

  public FileMetadata(String MD5hash, String path, long size, FileState state) {
    this.MD5hash = MD5hash;
    this.localPath = new File(path);
    this.size = size;
    this.bufferMap = "";
    this.state = state;
  }

  public FileMetadata(String MD5hash, String path, long size) {
    this(MD5hash, path, size, FileState.NONE);
  }

  public String getFileName() { return localPath.getName(); }
  public String getHash() { return MD5hash; }
  public long getSize() { return size; }
  public void setSize(long size) { this.size = size; }
  public File getlocalPath() { return localPath; }
  public void setlocalPath(File file) { this.localPath = file; }
  public void setPath(String newPath) { this.localPath = new File(newPath); }
  public String getBufferMap() { return bufferMap; }
  public void setBufferMap(String bufferMap) { this.bufferMap = bufferMap; }
  public FileState getState() { return state; }
  public void setState(FileState state) { this.state = state; }
}
