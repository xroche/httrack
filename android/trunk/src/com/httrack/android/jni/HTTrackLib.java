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

import java.io.IOException;

public class HTTrackLib {
	/** Statistics. **/
	protected final HTTrackCallbacks callbacks;

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
		this(null);
	}

	/**
	 * Constructor with statistics support.
	 */
	public HTTrackLib(HTTrackCallbacks callbacks) {
		this.callbacks = callbacks;
	}

	static {
		System.loadLibrary("httrack");
		System.loadLibrary("htslibjni");
	}
}
