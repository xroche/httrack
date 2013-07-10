package com.httrack.android.jni;

import java.io.IOException;

public class HTTrackLib {
  public static final long STATE_PARSING = 0;
  public static final long STATE_TESTING_LINKS = 1;
  public static final long STATE_PURGING_FILES = 2;
  public static final long STATE_LOADING_CACHE = 3;
  public static final long STATE_WAIT_SCHEDULER = 4;
  public static final long STATE_WAIT_THROTTLE = 5;

  /** State **/
  protected long state;

  /** For parsing, completion in percent (0..100) **/
  protected long completion;

  /** Network bytes received. **/
  protected long bytesReceived;

  /** Bytes written on disk. **/
  protected long bytesWritten;

  /** Start timestamp. **/
  protected long startTime;

  /** Bytes received compressed. **/
  protected long bytesReceivedCompressed;

  /** Bytes received uncompressed. **/
  protected long bytesReceivedUncompressed;

  /** Files received compressed. **/
  protected long filesReceivedCompressed;

  /** Files written. **/
  protected long filesWritten;

  /** Files updated. **/
  protected long filesUpdated;

  /** Files written in background. **/
  protected long filesWrittenBackground;

  /** Requests **/
  protected long requestsCount;

  /** Sockets allocated **/
  protected long socketsAllocated;

  /** Sockets count **/
  protected long socketsCount;

  /** Errors count **/
  protected long errorsCount;

  /** Warnings count **/
  protected long warningsCount;

  /** Info count **/
  protected long infosCount;

  /** Transfer rate overall **/
  protected long totalTransferRate;

  /** Current transfer rate **/
  protected long transferRate;

  /**
   * Get the current library version, as MAJOR.MINOR.SUBRELEASE string.
   * 
   * @return the current library version
   */
  public static native String getVersion();

  /**
   * Initialize the httrack library. MUST be called once.
   */
  public static native void init();

  /**
   * Start the engine.
   * 
   * @param args
   *          main() arguments.
   * @return The exit code upon completion.
   * @throws IOException
   *           upon error
   */
  public native int main(String[] args) throws IOException;

  /**
   * Stop the engine.
   */
  public native void stop(boolean force);

  /**
   * Default constructor.
   */
  public HTTrackLib() {
  }

  static {
    System.loadLibrary("httrack");
    System.loadLibrary("htslibjni");
  }
}
