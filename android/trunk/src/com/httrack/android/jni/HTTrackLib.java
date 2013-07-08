package com.httrack.android.jni;

import java.io.IOException;

public class HTTrackLib {

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
	 *            main() arguments.
	 * @return The exit code upon completion.
	 * @throws IOException
	 *             upon error
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
