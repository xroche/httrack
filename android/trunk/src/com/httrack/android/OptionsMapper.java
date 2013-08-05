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

package com.httrack.android;

import java.io.BufferedReader;
import java.io.File;
import java.io.FileReader;
import java.io.IOException;
import java.net.URLDecoder;
import java.util.HashMap;

import android.util.Pair;
import android.util.SparseArray;

/**
 * Options mapper, storing and managing options, and mapping GUI fields from/to
 * them.
 */
public class OptionsMapper {
  // Fields used in serialization
  // note: names tend to match the Windows winprofile.ini version
  @SuppressWarnings("unchecked")
  protected static final Pair<Integer, String> fieldsSerializer[] = new Pair[] {
      /* Global settings */
      new Pair<Integer, String>(R.id.fieldProjectName, "ProjectName"),
      new Pair<Integer, String>(R.id.fieldProjectCategory, "Category"),

      /* URL */
      new Pair<Integer, String>(R.id.fieldWebsiteURLs, "CurrentUrl"),
      new Pair<Integer, String>(R.id.radioAction, "CurrentAction"), /* FIXME */

      /* Scan Rules */
      new Pair<Integer, String>(R.id.editRules, "WildCardFilters"),

      /* Limits */
      new Pair<Integer, String>(R.id.editMaxDepth, "Depth"),
      new Pair<Integer, String>(R.id.editMaxExtDepth, "ExtDepth"),
      new Pair<Integer, String>(R.id.editMaxSizeHtml, "MaxHtml"),
      new Pair<Integer, String>(R.id.editMaxSizeOther, "MaxOther"),
      new Pair<Integer, String>(R.id.editSiteSizeLimit, "MaxAll"),
      new Pair<Integer, String>(R.id.editMaxTimeOverall, "MaxTime"),
      new Pair<Integer, String>(R.id.editMaxTransferRate, "MaxRate"),
      new Pair<Integer, String>(R.id.editMaxConnectionsSecond, "MaxConn"),
      new Pair<Integer, String>(R.id.editMaxNumberLinks, "MaxLinks"),

      /* Flow control */
      new Pair<Integer, String>(R.id.editNumberOfConnections, "Sockets"),
      new Pair<Integer, String>(R.id.checkPersistentConnections, "KeepAlive"),
      new Pair<Integer, String>(R.id.editTimeout, "TimeOut"),
      new Pair<Integer, String>(R.id.checkRemoveHostIfTimeout, "RemoveTimeout"),
      new Pair<Integer, String>(R.id.editRetries, "Retry"),
      new Pair<Integer, String>(R.id.editMinTransferRate, "RateOut"),
      new Pair<Integer, String>(R.id.checkRemoveHostIfSlow, "RemoveRateout"),

      /* Links */
      new Pair<Integer, String>(R.id.checkDetectAllLinks, "ParseAll"),
      new Pair<Integer, String>(R.id.checkGetNonHtmlNear, "Near"),
      new Pair<Integer, String>(R.id.checkTestAllLinks, "Test"),
      new Pair<Integer, String>(R.id.checkGetHtmlFirst, "HTMLFirst"),

      /* Build */
      new Pair<Integer, String>(R.id.checkDosNames, "Dos"),
      new Pair<Integer, String>(R.id.checkIso9660, "Iso9660"), /* FIXME with Dos */
      new Pair<Integer, String>(R.id.checkNoErrorPages, "NoErrorPages"),
      new Pair<Integer, String>(R.id.checkNoExternalPages, "NoExternalPages"),
      new Pair<Integer, String>(R.id.checkHidePasswords, "NoPwdInPages"),
      new Pair<Integer, String>(R.id.checkHideQueryStrings, "NoQueryStrings"),
      new Pair<Integer, String>(R.id.checkDoNotPurge, "NoPurgeOldFiles"),
      new Pair<Integer, String>(R.id.radioBuild, "Build"),

      /* Browser ID */
      new Pair<Integer, String>(R.id.editBrowserIdentity, "UserID"),
      new Pair<Integer, String>(R.id.editHtmlFooter, "Footer"),

      /* Spider */
      new Pair<Integer, String>(R.id.checkAcceptCookies, "Cookies"),
      new Pair<Integer, String>(R.id.radioCheckDocumentType, "CheckType"),
      new Pair<Integer, String>(R.id.checkParseJavaFiles, "ParseJava"),
      new Pair<Integer, String>(R.id.radioSpider, "FollowRobotsTxt"),
      new Pair<Integer, String>(R.id.checkUpdateHacks, "UpdateHack"),
      new Pair<Integer, String>(R.id.checkUrlHacks, "URLHack"),
      new Pair<Integer, String>(R.id.checkTolerentRequests, "TolerantRequests"),
      new Pair<Integer, String>(R.id.checkForceHttp10, "HTTP10"),

      /* Proxy */
      new Pair<Integer, String>(R.id.editProxy, "Proxy"),
      new Pair<Integer, String>(R.id.editProxyPort, "Port"),
      new Pair<Integer, String>(R.id.checkUseProxyForFtp, "UseHTTPProxyForFTP"),

      /* Log, Index, Cache */
      new Pair<Integer, String>(R.id.checkStoreAllFilesInCache,
          "StoreAllInCache"),
      new Pair<Integer, String>(R.id.checkDoNotRedownloadLocallErasedFiles,
          "NoRecatch"),
      new Pair<Integer, String>(R.id.checkCreateLogFiles, "Log"),
      /* FIXME with Log */
      new Pair<Integer, String>(R.id.radioVerbosity, "LogType"),
      new Pair<Integer, String>(R.id.checkUseIndex, "Index"),

      /* Experts Only */
      new Pair<Integer, String>(R.id.checkUseCacheForUpdates, "Cache"),
      new Pair<Integer, String>(R.id.radioPrimaryScanRule, "PrimaryScan"),
      new Pair<Integer, String>(R.id.textTravelMode, "Travel"),
      new Pair<Integer, String>(R.id.radioTravelMode, "GlobalTravel"),
      new Pair<Integer, String>(R.id.radioGlobalTravelMode, "GlobalTravel"),
      new Pair<Integer, String>(R.id.radioRewriteLinks, "RewriteLinks"),
      new Pair<Integer, String>(R.id.checkActivateDebugging, "Debugging"), /* FIXME */

  };

  // Default fields for empty profile
  @SuppressWarnings("unchecked")
  protected static final Pair<String, String> fieldsDefaults[] = new Pair[] {
      new Pair<String, String>("Near", "0"),
      new Pair<String, String>("Test", "0"),
      new Pair<String, String>("ParseAll", "1"),
      new Pair<String, String>("HTMLFirst", "0"),
      new Pair<String, String>("Cache", "1"),
      new Pair<String, String>("NoRecatch", "0"),
      new Pair<String, String>("Dos", "0"),
      new Pair<String, String>("Index", "1"),
      new Pair<String, String>("Log", "1"),
      new Pair<String, String>("RemoveTimeout", "0"),
      new Pair<String, String>("RemoveRateout", "0"),
      new Pair<String, String>("KeepAlive", "1"),
      new Pair<String, String>("FollowRobotsTxt", "2"),
      new Pair<String, String>("NoErrorPages", "0"),
      new Pair<String, String>("NoExternalPages", "0"),
      new Pair<String, String>("NoPwdInPages", "0"),
      new Pair<String, String>("NoQueryStrings", "0"),
      new Pair<String, String>("NoPurgeOldFiles", "0"),
      new Pair<String, String>("Cookies", "1"),
      new Pair<String, String>("CheckType", "1"),
      new Pair<String, String>("ParseJava", "1"),
      new Pair<String, String>("HTTP10", "0"),
      new Pair<String, String>("TolerantRequests", "0"),
      new Pair<String, String>("UpdateHack", "1"),
      new Pair<String, String>("URLHack", "1"),
      new Pair<String, String>("StoreAllInCache", "0"),
      new Pair<String, String>("LogType", "0"),
      new Pair<String, String>("UseHTTPProxyForFTP", "1"),
      new Pair<String, String>("Build", "0"),
      new Pair<String, String>("PrimaryScan", "3"),
      new Pair<String, String>("Travel", "1"),
      new Pair<String, String>("GlobalTravel", "0"),
      new Pair<String, String>("RewriteLinks", "0"),
      // new Pair<String, String>("BuildString", "%%h%%p/%%n%%q.%%t"),
      new Pair<String, String>("UserID",
          "Mozilla/4.5 (compatible; HTTrack 3.0x; Windows 98)"),
      new Pair<String, String>("Footer",
          "<!-- Mirrored from %%s%%s by HTTrack Website Copier/3.x [XR&CO'2013], %%s -->"),
      new Pair<String, String>("MaxRate", "25000"),
      new Pair<String, String>(
          "WildCardFilters",
          "+*.png +*.gif +*.jpg +*.css +*.js -ad.doubleclick.net/* -mime:application/foobar"),
  // new Pair<String, String>("CurrentAction", "0")
  };

  // Name-to-ID hash map
  protected static HashMap<String, Integer> fieldsNameToId = new HashMap<String, Integer>();

  // The options mapping
  protected final SparseArraySerializable map = new SparseArraySerializable();

  // Static initializer.
  static {
    // Create fieldsNameToId
    createFieldsNameToId();
  }

  /*
   * Build fieldsNameToId
   */
  private static void createFieldsNameToId() {
    // Create fieldsNameToId
    for (final Pair<Integer, String> field : fieldsSerializer) {
      final int id = field.first;
      final String fkey = field.second;
      fieldsNameToId.put(fkey, id);
    }
  }

  /*
   * Fill the map with default values
   */
  private void initializeMap() {
    for (final Pair<String, String> field : fieldsDefaults) {
      final Integer id = fieldsNameToId.get(field.first);
      if (id == null) {
        throw new RuntimeException("unexpecter error: unknown key "
            + field.first);
      }
      map.put(id, field.second);
    }
  }

  /**
   * Build a new options mapper.
   */
  public OptionsMapper() {
    // Initialize default values for map
    initializeMap();
  }

  /**
   * Return the underlying map.
   * 
   * @return The underlying map.
   */
  public SparseArraySerializable getMap() {
    return map;
  }

  /**
   * Get a map value.
   * 
   * @param key
   *          The key
   * @return The value
   */
  public String getMap(int key) {
    return map.get(key);
  }

  /**
   * Set a map value.
   * 
   * @param key
   *          The key
   * @param value
   *          The value
   */
  public void setMap(int key, final String value) {
    map.put(key, value);
  }

  /**
   * Unserialize a specific profile from disk.
   * 
   * @param profile
   *          The profile file (winprofile.ini)
   * @param map
   *          The map to be filled
   * 
   * @throws IOException
   *           Upon I/O error.
   */
  protected static void unserialize(final File profile,
      final SparseArray<String> map) throws IOException {
    // Write settings
    if (!profile.exists()) {
      throw new IOException("no such profile");
    }
    final FileReader reader = new FileReader(profile);
    final BufferedReader lreader = new BufferedReader(reader);
    try {
      String rawline;
      while ((rawline = lreader.readLine()) != null) {
        final String line = rawline.trim();
        if (line.length() == 0 || line.charAt(0) == ';') {
          continue;
        }
        final int sep = line.indexOf('=');
        if (sep != -1) {
          final String key = line.substring(0, sep);
          final String value = URLDecoder.decode(line.substring(sep + 1),
              "UTF-8");
          final Integer id = OptionsMapper.fieldsNameToId.get(key);
          if (id != null) {
            map.put(id, value);
          }
        }
      }
      lreader.close();
    } finally {
      reader.close();
    }
  }

  /**
   * Unserialize profile from disk.
   * 
   * @throws IOException
   *           Upon I/O error.
   */
  protected void unserialize(final File profile) throws IOException {
    unserialize(profile, map);
  }
}
