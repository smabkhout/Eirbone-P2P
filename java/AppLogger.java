import java.io.File;
import java.io.FileWriter;
import java.io.IOException;
import java.io.PrintWriter;
import java.time.LocalDateTime;
import java.time.format.DateTimeFormatter;

public final class AppLogger {
  public enum Level { ERROR, WARN, INFO }

  private static final Object LOCK = new Object();
  private static final DateTimeFormatter FORMATTER =
      DateTimeFormatter.ofPattern("yyyy-MM-dd HH:mm:ss");

  private static volatile Level level = Level.INFO;
  private static volatile PrintWriter fileWriter;

  private AppLogger() {}

  public static void configure(String logFilePath, String levelName) {
    synchronized (LOCK) {
      closeWriterLocked();
      level = parseLevel(levelName);
      if (logFilePath == null || logFilePath.trim().isEmpty()) {
        fileWriter = null;
        return;
      }

      try {
        File logFile = new File(logFilePath.trim());
        File parent = logFile.getParentFile();
        if (parent != null && !parent.exists()) {
          parent.mkdirs();
        }
        fileWriter = new PrintWriter(new FileWriter(logFile, true), true);
      } catch (IOException e) {
        fileWriter = null;
        System.err.println("[WARN] Unable to open log file: " + e.getMessage());
      }
    }
  }

  public static void close() {
    synchronized (LOCK) {
      closeWriterLocked();
    }
  }

  public static void error(String message) {
    log(Level.ERROR, message);
  }

  public static void warn(String message) {
    log(Level.WARN, message);
  }

  public static void info(String message) {
    log(Level.INFO, message);
  }

  private static void log(Level messageLevel, String message) {
    if (message == null || messageLevel.ordinal() > level.ordinal()) {
      return;
    }

    String timestamp = LocalDateTime.now().format(FORMATTER);
    String line = "[" + timestamp + "] [" + messageLevel + "] " + message;
    System.out.println(line);

    synchronized (LOCK) {
      if (fileWriter != null) {
        fileWriter.println(line);
        fileWriter.flush();
      }
    }
  }

  private static Level parseLevel(String levelName) {
    if (levelName == null) {
      return Level.INFO;
    }
    try {
      return Level.valueOf(levelName.trim().toUpperCase());
    } catch (IllegalArgumentException e) {
      return Level.INFO;
    }
  }

  private static void closeWriterLocked() {
    if (fileWriter != null) {
      fileWriter.close();
      fileWriter = null;
    }
  }
}