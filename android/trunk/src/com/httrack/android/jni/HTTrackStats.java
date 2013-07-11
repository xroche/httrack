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

/**
 * HTTrack statistics class.
 **/
public class HTTrackStats {
  /** Elements being processed currently. **/
  public static class Element {
    /** Unknown state. **/
    public static final int STATE_NONE = 0;

    /** Receiving data. **/
    public static final int STATE_RECEIVE = 1;

    /** Connecting. **/
    public static final int STATE_CONNECTING = 2;

    /** Resolving DNS. **/
    public static final int STATE_DNS = 3;

    /** FTP operation. **/
    public static final int STATE_FTP = 4;

    /** Ready. **/
    public static final int STATE_READY = 5;

    /** Address **/
    public String address;

    /** Path (including query-string) **/
    public String path;

    /** Destination filename on disk (or null) **/
    public String filename;

    /** File has previously been downloaded. **/
    public boolean isUpdate;

    /** Item state (see STATE_* fields) **/
    public int state;

    /** HTTP status code, if suitable. **/
    public int code;

    /** HTTP status message, if suitable. **/
    public String message;

    /** File was not modified. **/
    public boolean isNotModified;

    /** File was compressed. **/
    public boolean isCompressed;

    /** File size (< 0 if unknown) **/
    public long size;

    /** File total size (< 0 if unknown) **/
    public long totalSize;

    /** MIME type **/
    public String mime;

    /** Charset **/
    public String charset;
  }

  /** Engine is parsing. **/
  public static final long STATE_PARSING = 0;

  /** Engine is testing links. **/
  public static final long STATE_TESTING_LINKS = 1;

  /** Engine is purgine files at the end of the mirror. **/
  public static final long STATE_PURGING_FILES = 2;

  /** Engine is loading cache. **/
  public static final long STATE_LOADING_CACHE = 3;

  /** Engine is waiting for starting time. **/
  public static final long STATE_WAIT_SCHEDULER = 4;

  /** Engine is throttling. **/
  public static final long STATE_WAIT_THROTTLE = 5;

  /** State **/
  public long state;

  /** For parsing, completion in percent (0..100) **/
  public long completion;

  /** Network bytes received. **/
  public long bytesReceived;

  /** Bytes written on disk. **/
  public long bytesWritten;

  /** Start timestamp. **/
  public long startTime;

  /** Bytes received compressed. **/
  public long bytesReceivedCompressed;

  /** Bytes received uncompressed. **/
  public long bytesReceivedUncompressed;

  /** Files received compressed. **/
  public long filesReceivedCompressed;

  /** Files written. **/
  public long filesWritten;

  /** Files updated. **/
  public long filesUpdated;

  /** Files written in background. **/
  public long filesWrittenBackground;

  /** Requests **/
  public long requestsCount;

  /** Sockets allocated **/
  public long socketsAllocated;

  /** Sockets count **/
  public long socketsCount;

  /** Errors count **/
  public long errorsCount;

  /** Warnings count **/
  public long warningsCount;

  /** Info count **/
  public long infosCount;

  /** Transfer rate overall **/
  public long totalTransferRate;

  /** Current transfer rate **/
  public long transferRate;

  /** Elements currently in progress. **/
  public Element[] elements;
}
