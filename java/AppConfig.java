import java.io.File;
import java.io.FileInputStream;
import java.io.IOException;
import java.util.Properties;

public final class AppConfig {
  private final String trackerAddress;
  private final int trackerPort;
  private final long peerUpdateIntervalMs;
  private final long trackerUpdateIntervalMs;
  private final String logFile;
  private final String logLevel;

  private AppConfig(String trackerAddress, int trackerPort,
                    long peerUpdateIntervalMs, long trackerUpdateIntervalMs,
                    String logFile, String logLevel) {
    this.trackerAddress = trackerAddress;
    this.trackerPort = trackerPort;
    this.peerUpdateIntervalMs = peerUpdateIntervalMs;
    this.trackerUpdateIntervalMs = trackerUpdateIntervalMs;
    this.logFile = logFile;
    this.logLevel = logLevel;
  }

  public String getTrackerAddress() { return trackerAddress; }
  public int getTrackerPort() { return trackerPort; }
  public long getPeerUpdateIntervalMs() { return peerUpdateIntervalMs; }
  public long getTrackerUpdateIntervalMs() { return trackerUpdateIntervalMs; }
  public String getLogFile() { return logFile; }
  public String getLogLevel() { return logLevel; }

  public static AppConfig loadDefault() {
    return loadFromFile(findConfigFile());
  }

  public static AppConfig loadFromFile(File file) {
    Properties props = new Properties();
    if (file != null && file.exists()) {
      try (FileInputStream in = new FileInputStream(file)) {
        props.load(in);
      } catch (IOException e) {
        System.err.println("[WARN] Unable to read config file: " + e.getMessage());
      }
    }

    String trackerAddress = props.getProperty("tracker-address", "127.0.0.1").trim();
    int trackerPort = parseInt(props.getProperty("tracker-port"), 12345);
    long peerUpdateIntervalMs = parseLong(props.getProperty("peer-update-interval-ms"), 5000L);
    long trackerUpdateIntervalMs = parseLong(props.getProperty("tracker-update-interval-ms"), 10000L);
    String logFile = props.getProperty("log-file", "peer.log").trim();
    String logLevel = props.getProperty("log-level", "INFO").trim();

    return new AppConfig(trackerAddress, trackerPort, peerUpdateIntervalMs,
                         trackerUpdateIntervalMs, logFile, logLevel);
  }

  private static File findConfigFile() {
    File[] candidates = {
        new File("config.ini"),
        new File("java", "config.ini")
    };
    for (File candidate : candidates) {
      if (candidate.exists()) {
        return candidate;
      }
    }
    return candidates[0];
  }

  private static int parseInt(String value, int defaultValue) {
    try {
      return value == null ? defaultValue : Integer.parseInt(value.trim());
    } catch (NumberFormatException e) {
      return defaultValue;
    }
  }

  private static long parseLong(String value, long defaultValue) {
    try {
      return value == null ? defaultValue : Long.parseLong(value.trim());
    } catch (NumberFormatException e) {
      return defaultValue;
    }
  }
}