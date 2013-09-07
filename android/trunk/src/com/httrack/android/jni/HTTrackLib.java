/*
HTTrack Android Java Interface.

HTTrack Website Copier, Offline Browser for Windows and Unix
Copyright (C) Xavier Roche and other contributors

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 3
of the License, or any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 */

package com.httrack.android.jni;

import java.io.File;
import java.io.IOException;

public class HTTrackLib {
  /** Opaque native context. **/
  private long nativeObject;

  /** Statistics. **/
  protected final HTTrackCallbacks callbacks;

  /**
   * Get the current library version, as MAJOR.MINOR.SUBRELEASE string.
   * 
   * @return the current library version
   */
  public static native String getVersion();

  /**
   * Get the current library features, as a string of [+-]tag.
   * 
   * @return the current library features.
   */
  public static native String getFeatures();

  /**
   * Build the top-level index.
   * 
   * @param path
   *          The target path;
   * @param templatesPath
   *          The templates path directory.
   * @return 1 upon success
   */
  public static int buildTopIndex(final File path, final File templatesPath) {
    final String p = path.getAbsolutePath() + "/";
    final String t = templatesPath.getAbsolutePath() + "/";
    return buildTopIndex(p, t);
  }

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
   * 
   * @return true if the engine was stopped (or at least a request was sent),
   *         false if it has already stopped.
   */
  public native boolean stop(boolean force);

  /**
   * Default constructor.
   */
  public HTTrackLib() {
    this(null);
  }

  /**
   * Constructor with statistics support.
   */
  public HTTrackLib(final HTTrackCallbacks callbacks) {
    init();
    this.callbacks = callbacks;
  }

  @Override
  public void finalize() {
    free();
  }

  /*
   * Initialize.
   */
  protected native void init() throws RuntimeException;

  /*
   * Static initialization.
   */
  private native static void initStatic() throws RuntimeException;

  /*
   * Free.
   */
  protected native void free();

  protected static native int buildTopIndex(String path, String templatesPath);

  static {
    /**
     * Load needed native libraries. Remember that we do not have our library
     * path in the standard library path, and therefore loading "htslibjni" will
     * just fail because dependencies would not be found. Instead, we have to
     * load in reverse topological order all dependencies, and only load the
     * final JNI stub at the end. All libraries, except the final JNI stub, will
     * be loaded without JNI_OnLoad() being called (because not present for
     * obvious reasons), and this is perfectly fine.
     */

    /*
     * Android seems to have both /system/lib/libcrypto.so and
     * /system/lib/libssl.so, and Java loads them in priority when using
     * System.loadLibrary(...) We should either ensure that the system libs are
     * fine (they seems fine BTW) or explicitly load the
     * application-cached-library using some dirty magic (ie. giving the
     * explicit full path and .so) For now, we do not ship the libraries, which
     * will spare some space, and hope the ABI won't change too much.
     */
    System.loadLibrary("crypto");
    System.loadLibrary("ssl");

    /** Iconv (Unicode). **/
    System.loadLibrary("iconv");

    /** HTTrack core engine. **/
    System.loadLibrary("httrack");

    /**
     * HTTrack Java plugin (note: dlopen()'ed by HTTrack, has symbol
     * dependencies to libhttrack.so).
     **/
    System.loadLibrary("htsjava");

    /** HTTrack Android JNI layer. **/
    System.loadLibrary("htslibjni");

    /** Static initialization. Throws RuntimeException upon error. **/
    initStatic();
  }
}
