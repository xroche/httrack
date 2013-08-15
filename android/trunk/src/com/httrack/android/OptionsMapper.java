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
import java.io.BufferedWriter;
import java.io.File;
import java.io.FileReader;
import java.io.FileWriter;
import java.io.IOException;
import java.io.UnsupportedEncodingException;
import java.util.ArrayList;
import java.util.HashMap;
import java.util.List;

import android.content.Context;
import android.os.Parcelable;
import android.util.Log;
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
      new Pair<Integer, String>(R.id.editCustomBuild, "BuildString"),

      /* Browser ID */
      new Pair<Integer, String>(R.id.editBrowserIdentity, "UserID"),
      new Pair<Integer, String>(R.id.editHtmlFooter, "Footer"),
      new Pair<Integer, String>(R.id.editAcceptLanguage, "AcceptLanguage"),
      new Pair<Integer, String>(R.id.editOtherHeaders, "OtherHeaders"),

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

      /*
       * Type/MIME associations (lame mapping, but compatible with the
       * WinHTTrack one)
       */
      new Pair<Integer, String>(R.id.editExtDef1, "MIMEDefsExt1"),
      new Pair<Integer, String>(R.id.editMimeDef1, "MIMEDefsMime1"),
      new Pair<Integer, String>(R.id.editExtDef2, "MIMEDefsExt2"),
      new Pair<Integer, String>(R.id.editMimeDef2, "MIMEDefsMime2"),
      new Pair<Integer, String>(R.id.editExtDef3, "MIMEDefsExt3"),
      new Pair<Integer, String>(R.id.editMimeDef3, "MIMEDefsMime3"),
      new Pair<Integer, String>(R.id.editExtDef4, "MIMEDefsExt4"),
      new Pair<Integer, String>(R.id.editMimeDef4, "MIMEDefsMime4"),
      new Pair<Integer, String>(R.id.editExtDef5, "MIMEDefsExt5"),
      new Pair<Integer, String>(R.id.editMimeDef5, "MIMEDefsMime5"),
      new Pair<Integer, String>(R.id.editExtDef6, "MIMEDefsExt6"),
      new Pair<Integer, String>(R.id.editMimeDef6, "MIMEDefsMime6"),
      new Pair<Integer, String>(R.id.editExtDef7, "MIMEDefsExt7"),
      new Pair<Integer, String>(R.id.editMimeDef7, "MIMEDefsMime7"),
      new Pair<Integer, String>(R.id.editExtDef8, "MIMEDefsExt8"),
      new Pair<Integer, String>(R.id.editMimeDef8, "MIMEDefsMime8"),

      /* Experts Only */
      new Pair<Integer, String>(R.id.checkUseCacheForUpdates, "Cache"),
      new Pair<Integer, String>(R.id.radioPrimaryScanRule, "PrimaryScan"),
      new Pair<Integer, String>(R.id.radioTravelMode, "Travel"),
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
      new Pair<String, String>("BuildString", "%h%p/%n%q.%t"),
      new Pair<String, String>("UserID",
          "Mozilla/4.5 (compatible; HTTrack 3.0x; Windows 98)"),
      new Pair<String, String>("Footer",
          "<!-- Mirrored from %s%s by HTTrack Website Copier/3.x [XR&CO'2013], %s -->"),
      new Pair<String, String>("AcceptLanguage", "en,*"),
      new Pair<String, String>("OtherHeaders", ""),
      new Pair<String, String>("MaxRate", "25000"),
      new Pair<String, String>(
          "WildCardFilters",
          "+*.png +*.gif +*.jpg +*.css +*.js -ad.doubleclick.net/* -mime:application/foobar"),
  // new Pair<String, String>("CurrentAction", "0")
  };

  // Special options to be handled/merged differently
  protected final MaxSizeHandler maxSizeHandler = new MaxSizeHandler();
  protected final HostControlHandler hostControlHandler = new HostControlHandler();
  protected final DosIso9660Handler dosIso9660Handler = new DosIso9660Handler();
  protected final BuildHandler buildHandler = new BuildHandler();
  protected final ProxyHandler proxyHandler = new ProxyHandler();
  protected final LogHandler logHandler = new LogHandler();
  protected final PrimaryScanHandler primaryScanHandler = new PrimaryScanHandler();
  protected final MimeTypesHandler[] mimeTypesHandlers = new MimeTypesHandler[] {
      new MimeTypesHandler(), new MimeTypesHandler(), new MimeTypesHandler(),
      new MimeTypesHandler(), new MimeTypesHandler(), new MimeTypesHandler(),
      new MimeTypesHandler(), new MimeTypesHandler() };

  // Fields mapper to httrack engine options
  @SuppressWarnings("unchecked")
  protected final Pair<String, OptionMapper> fieldsMapper[] = new Pair[] {
      new Pair<String, OptionMapper>("ProjectName", NoOpOption.INSTANCE),
      new Pair<String, OptionMapper>("Category", NoOpOption.INSTANCE),
      new Pair<String, OptionMapper>("CurrentUrl", StringSplit.INSTANCE),
      new Pair<String, OptionMapper>("CurrentAction",
          new MultipleChoicesOption(new String[] { "iC1", "iC2" }, true)),
      new Pair<String, OptionMapper>("WildCardFilters", StringSplit.INSTANCE),
      new Pair<String, OptionMapper>("Depth", new SimpleOption("r")),
      new Pair<String, OptionMapper>("ExtDepth", new SimpleOption("%e")),
      new Pair<String, OptionMapper>("MaxHtml", maxSizeHandler.getHtml()),
      new Pair<String, OptionMapper>("MaxOther", maxSizeHandler.getNonHtml()),
      new Pair<String, OptionMapper>("MaxAll", new SimpleOption("M")),
      new Pair<String, OptionMapper>("MaxTime", new SimpleOption("E")),
      new Pair<String, OptionMapper>("MaxRate", new SimpleOption("A")),
      new Pair<String, OptionMapper>("MaxConn", new SimpleOption("%c")),
      new Pair<String, OptionMapper>("MaxLinks", new SimpleOption("#L")),
      new Pair<String, OptionMapper>("Sockets", new SimpleOption("c")),
      new Pair<String, OptionMapper>("KeepAlive", new SimpleOption0("k")),
      new Pair<String, OptionMapper>("TimeOut", new SimpleOption("T")),
      new Pair<String, OptionMapper>("RemoveTimeout",
          hostControlHandler.getTimeMapper()),
      new Pair<String, OptionMapper>("RemoveRateout",
          hostControlHandler.getRateMapper()),
      new Pair<String, OptionMapper>("Retry", new SimpleOption("R")),
      new Pair<String, OptionMapper>("RateOut", new SimpleOption("J")),
      new Pair<String, OptionMapper>("ParseAll", new SimpleOption0("%P")),
      new Pair<String, OptionMapper>("Near", new SimpleOptionFlag("n")),
      new Pair<String, OptionMapper>("Test", new SimpleOptionFlag("t")),
      new Pair<String, OptionMapper>("HTMLFirst",
          primaryScanHandler.getHtmlFirstMapper()),
      new Pair<String, OptionMapper>("Dos", dosIso9660Handler.getDosMapper()),
      new Pair<String, OptionMapper>("Iso9660",
          dosIso9660Handler.getIso9660Mapper()),
      new Pair<String, OptionMapper>("NoErrorPages", new SimpleOptionFlag("o0")),
      new Pair<String, OptionMapper>("NoExternalPages", new SimpleOptionFlag(
          "x", true)),
      new Pair<String, OptionMapper>("NoPwdInPages", new SimpleOptionFlag("%x")),
      new Pair<String, OptionMapper>("NoQueryStrings", new SimpleOption0("%q")),
      new Pair<String, OptionMapper>("NoPurgeOldFiles", new SimpleOptionFlag(
          "X0")),
      new Pair<String, OptionMapper>("Build", buildHandler.getTypeMapper()),
      new Pair<String, OptionMapper>("BuildString",
          buildHandler.getCustomMapper()),
      new Pair<String, OptionMapper>("UserID", new ArgumentOption("-F")),
      new Pair<String, OptionMapper>("Footer", new ArgumentOption("-%F")),
      new Pair<String, OptionMapper>("AcceptLanguage",
          new ArgumentOption("-%l")),
      new Pair<String, OptionMapper>("OtherHeaders", new StringSplit("-%X")),
      new Pair<String, OptionMapper>("Cookies",
          new SimpleOptionFlag("b0", true)),
      new Pair<String, OptionMapper>("CheckType", new SimpleOption("u")),
      new Pair<String, OptionMapper>("ParseJava", new SimpleOptionFlag("j",
          true)),
      new Pair<String, OptionMapper>("FollowRobotsTxt", new SimpleOption("s")),
      new Pair<String, OptionMapper>("UpdateHack", new SimpleOptionFlag("%s")),
      new Pair<String, OptionMapper>("URLHack", new SimpleOption0("%u")),
      new Pair<String, OptionMapper>("TolerantRequests", new SimpleOptionFlag(
          "%B")),
      new Pair<String, OptionMapper>("HTTP10", new SimpleOptionFlag("%h")),
      new Pair<String, OptionMapper>("Proxy", proxyHandler.getAddressMapper()),
      new Pair<String, OptionMapper>("Port", proxyHandler.getPortMapper()),
      new Pair<String, OptionMapper>("UseHTTPProxyForFTP", new SimpleOption0(
          "%f")),
      new Pair<String, OptionMapper>("StoreAllInCache", new SimpleOptionFlag(
          "k")),
      new Pair<String, OptionMapper>("NoRecatch", new SimpleOptionFlag("%n")),
      new Pair<String, OptionMapper>("Log", logHandler.getEnabledMapper()),
      new Pair<String, OptionMapper>("LogType", logHandler.getTypeMapper()),
      new Pair<String, OptionMapper>("Index", new SimpleOptionFlag("I0", true)),
      new Pair<String, OptionMapper>("Cache", new SimpleOptionFlag("C0", true)), /* FIXME */
      new Pair<String, OptionMapper>("PrimaryScan",
          primaryScanHandler.getTypeMapper()),
      new Pair<String, OptionMapper>("Travel", new MultipleChoicesOption(
          new String[] { "S", "D", "U", "B" }, true)),
      new Pair<String, OptionMapper>("GlobalTravel", new MultipleChoicesOption(
          new String[] { "a", "d", "l", "e" }, true)),
      new Pair<String, OptionMapper>("RewriteLinks", new MultipleChoicesOption(
          new String[] { "K0", "K", "K3", "K4" }, true)),
      new Pair<String, OptionMapper>("Debugging", new SimpleOptionFlag("%H")),
      new Pair<String, OptionMapper>("MIMEDefsExt1",
          mimeTypesHandlers[0].getExtMapper()),
      new Pair<String, OptionMapper>("MIMEDefsMime1",
          mimeTypesHandlers[0].getMimeMapper()),
      new Pair<String, OptionMapper>("MIMEDefsExt2",
          mimeTypesHandlers[1].getExtMapper()),
      new Pair<String, OptionMapper>("MIMEDefsMime2",
          mimeTypesHandlers[1].getMimeMapper()),
      new Pair<String, OptionMapper>("MIMEDefsExt3",
          mimeTypesHandlers[2].getExtMapper()),
      new Pair<String, OptionMapper>("MIMEDefsMime3",
          mimeTypesHandlers[2].getMimeMapper()),
      new Pair<String, OptionMapper>("MIMEDefsExt4",
          mimeTypesHandlers[3].getExtMapper()),
      new Pair<String, OptionMapper>("MIMEDefsMime4",
          mimeTypesHandlers[3].getMimeMapper()),
      new Pair<String, OptionMapper>("MIMEDefsExt5",
          mimeTypesHandlers[4].getExtMapper()),
      new Pair<String, OptionMapper>("MIMEDefsMime5",
          mimeTypesHandlers[4].getMimeMapper()),
      new Pair<String, OptionMapper>("MIMEDefsExt6",
          mimeTypesHandlers[5].getExtMapper()),
      new Pair<String, OptionMapper>("MIMEDefsMime6",
          mimeTypesHandlers[5].getMimeMapper()),
      new Pair<String, OptionMapper>("MIMEDefsExt7",
          mimeTypesHandlers[6].getExtMapper()),
      new Pair<String, OptionMapper>("MIMEDefsMime7",
          mimeTypesHandlers[6].getMimeMapper()),
      new Pair<String, OptionMapper>("MIMEDefsExt8",
          mimeTypesHandlers[7].getExtMapper()),
      new Pair<String, OptionMapper>("MIMEDefsMime8",
          mimeTypesHandlers[7].getMimeMapper()) };

  // Name-to-ID hash map
  protected static HashMap<String, Integer> fieldsNameToId = new HashMap<String, Integer>();

  // String-to-OptionMapper map
  protected HashMap<String, OptionMapper> fieldsNameToMapper = new HashMap<String, OptionMapper>();

  // The options mapping
  protected final SparseArraySerializable map = new SparseArraySerializable();

  // Is the map dirty ?
  protected boolean dirty;

  // Current context
  protected Context context;

  // Static initializer.
  static {
    // Create fieldsNameToId
    createFieldsNameToId();
  }

  /**
   * Options mapper interface.
   */
  public static interface OptionMapper {
    /**
     * Optional interface to OptionMapper allowing to execute post-action.
     */
    public static interface FinishMapper {
      /**
       * FinisExecute post-action
       * 
       * @param flags
       *          the flags array
       * @param commandline
       *          the commandline array
       */
      public void finish(final StringBuilder flags,
          final List<String> commandline);
    }

    /**
     * Emit the option
     * 
     * @param flags
     *          the flags array
     * @param commandline
     *          the commandline array
     * @param value
     *          the option value
     */
    public void emit(final StringBuilder flags, final List<String> commandline,
        final String value);
  }

  /**
   * No-op mapper.
   */
  public static class NoOpOption implements OptionMapper {
    /**
     * An instance of the NoOpOption class.
     */
    public static final NoOpOption INSTANCE = new NoOpOption();

    @Override
    public void emit(final StringBuilder flags, final List<String> commandline,
        final String value) {
    }
  }

  /**
   * Split into string pieces.
   */
  public static class StringSplit implements OptionMapper {
    /**
     * An instance of the basic StringSplit class.
     */
    public static final StringSplit INSTANCE = new StringSplit();

    protected final String option;

    /**
     * Default constructor. Splits lines.
     */
    public StringSplit() {
      this(null);
    }

    /**
     * Option-enabled feature ; when an option before each splitted string is
     * required.
     * 
     * @param option
     *          The option
     */
    public StringSplit(final String option) {
      this.option = option;
    }

    @Override
    public void emit(final StringBuilder flags, final List<String> commandline,
        final String value) {
      // URLs
      for (final String s : cleanupString(value).trim().split("\\s+")) {
        if (s.length() != 0) {
          if (option != null) {
            commandline.add(option);
          }
          commandline.add(s);
        }
      }
    }
  }

  /**
   * Maximum size handler. Merge the two maximum size of html/non-html files
   * options.
   */
  public static class MaxSizeHandler {
    protected boolean finished = false;
    protected int maxHtml = -1;
    protected int maxNonHtml = -1;

    /*
     * Handle "abandon host if timeout"
     */
    private class Html implements OptionMapper, OptionMapper.FinishMapper {
      @Override
      public void emit(final StringBuilder flags,
          final List<String> commandline, final String value) {
        if (value != null && value.length() != 0) {
          MaxSizeHandler.this.maxHtml = Integer.parseInt(value);
        }
      }

      @Override
      public void finish(final StringBuilder flags,
          final List<String> commandline) {
        MaxSizeHandler.this.finish(flags, commandline);
      }
    }

    /*
     * Handle "abandon host if transfer rate too low"
     */
    private class NonHtml implements OptionMapper, OptionMapper.FinishMapper {
      @Override
      public void emit(final StringBuilder flags,
          final List<String> commandline, final String value) {
        if (value != null && value.length() != 0) {
          MaxSizeHandler.this.maxNonHtml = Integer.parseInt(value);
        }
      }

      @Override
      public void finish(final StringBuilder flags,
          final List<String> commandline) {
        MaxSizeHandler.this.finish(flags, commandline);
      }
    }

    /**
     * Get the html size mapper
     * 
     * @return the html size mapper
     */
    public OptionMapper getHtml() {
      return new Html();
    }

    /**
     * Get the non-html size mapper
     * 
     * @return the non-html size mapper
     */
    public OptionMapper getNonHtml() {
      return new NonHtml();
    }

    /*
     * Where we really emit the option
     */
    private void finish(final StringBuilder flags,
        final List<String> commandline) {
      // mN maximum file length for a non-html file (--max-files[=N])
      // mN,N2 maximum file length for non html (N) and html (N2)
      if (!finished) {
        finished = true;
        if (maxNonHtml != -1 || maxHtml != -1) {
          flags.append("m");
          if (maxNonHtml != -1) {
            flags.append(maxNonHtml);
          }
          if (maxHtml != -1) {
            flags.append(',');
            flags.append(maxHtml);
          }
        }
      }
    }
  }

  /**
   * Host control handler. Merge the two
   * "abandon host if timeout/transfer rate too slow" options into a single one.
   */
  public static class HostControlHandler {
    protected boolean finished = false;
    protected int flag = 0;

    /*
     * Handle "abandon host if timeout"
     */
    private class Time implements OptionMapper, OptionMapper.FinishMapper {
      @Override
      public void emit(final StringBuilder flags,
          final List<String> commandline, final String value) {
        if ("1".equals(value)) {
          HostControlHandler.this.flag |= 1;
        }
      }

      @Override
      public void finish(final StringBuilder flags,
          final List<String> commandline) {
        HostControlHandler.this.finish(flags, commandline);
      }
    }

    /*
     * Handle "abandon host if transfer rate too low"
     */
    private class Rate implements OptionMapper, OptionMapper.FinishMapper {
      @Override
      public void emit(final StringBuilder flags,
          final List<String> commandline, final String value) {
        if ("1".equals(value)) {
          HostControlHandler.this.flag |= 2;
        }
      }

      @Override
      public void finish(final StringBuilder flags,
          final List<String> commandline) {
        HostControlHandler.this.finish(flags, commandline);
      }
    }

    /**
     * Get the transfer rate mapper
     * 
     * @return the transfer rate mapper
     */
    public OptionMapper getRateMapper() {
      return new Rate();
    }

    /**
     * Get the timeout mapper
     * 
     * @return the timeout mapper
     */
    public OptionMapper getTimeMapper() {
      return new Time();
    }

    /*
     * Where we really emit the option
     */
    private void finish(final StringBuilder flags,
        final List<String> commandline) {
      if (!finished) {
        finished = true;
        flags.append("H");
        flags.append(flag);
      }
    }
  }

  /**
   * DOS/ISO9660 flags.
   */
  public static class DosIso9660Handler {
    protected boolean finished = false;
    protected boolean dos = false;
    protected boolean iso9660 = false;

    /*
     * Handle "abandon host if timeout"
     */
    private class Dos implements OptionMapper, OptionMapper.FinishMapper {
      @Override
      public void emit(final StringBuilder flags,
          final List<String> commandline, final String value) {
        if ("1".equals(value)) {
          DosIso9660Handler.this.dos = true;
        }
      }

      @Override
      public void finish(final StringBuilder flags,
          final List<String> commandline) {
        DosIso9660Handler.this.finish(flags, commandline);
      }
    }

    /*
     * Handle "abandon host if transfer rate too low"
     */
    private class Iso9660 implements OptionMapper, OptionMapper.FinishMapper {
      @Override
      public void emit(final StringBuilder flags,
          final List<String> commandline, final String value) {
        if ("1".equals(value)) {
          DosIso9660Handler.this.iso9660 = true;
        }
      }

      @Override
      public void finish(final StringBuilder flags,
          final List<String> commandline) {
        DosIso9660Handler.this.finish(flags, commandline);
      }
    }

    /**
     * Get the dos flag mapper
     * 
     * @return the dos flag mapper
     */
    public OptionMapper getDosMapper() {
      return new Dos();
    }

    /**
     * Get the iso9660 flag mapper
     * 
     * @return the iso9660 flag mapper
     */
    public OptionMapper getIso9660Mapper() {
      return new Iso9660();
    }

    /*
     * Where we really emit the option
     */
    private void finish(final StringBuilder flags,
        final List<String> commandline) {
      if (!finished) {
        finished = true;
        // Note: same logic (...) as WinHTTrack ; see WinHTTrack/Shell.cpp
        if (dos || iso9660) {
          flags.append("L");
          if (dos) {
            flags.append('0');
          } else {
            flags.append('2');
          }
        }
      }
    }
  }

  /**
   * Proxy settings handler.
   */
  public static class ProxyHandler {
    protected boolean finished = false;
    protected String address;
    protected String port;

    /*
     * Build type.
     */
    private class Address implements OptionMapper, OptionMapper.FinishMapper {
      @Override
      public void emit(final StringBuilder flags,
          final List<String> commandline, final String value) {
        if (value != null && value.length() != 0) {
          ProxyHandler.this.address = value;
        }
      }

      @Override
      public void finish(final StringBuilder flags,
          final List<String> commandline) {
        ProxyHandler.this.finish(flags, commandline);
      }
    }

    /*
     * Custom build
     */
    private class Port implements OptionMapper, OptionMapper.FinishMapper {
      @Override
      public void emit(final StringBuilder flags,
          final List<String> commandline, final String value) {
        if (value != null && value.length() != 0) {
          ProxyHandler.this.port = value;
        }
      }

      @Override
      public void finish(final StringBuilder flags,
          final List<String> commandline) {
        ProxyHandler.this.finish(flags, commandline);
      }
    }

    /**
     * Get address mapper
     * 
     * @return the address mapper
     */
    public OptionMapper getAddressMapper() {
      return new Address();
    }

    /**
     * Get the port mapper
     * 
     * @return the port build mapper
     */
    public OptionMapper getPortMapper() {
      return new Port();
    }

    /*
     * Where we really emit the option
     */
    private void finish(final StringBuilder flags,
        final List<String> commandline) {
      if (!finished) {
        finished = true;
        if (address != null) {
          commandline.add("-P");
          commandline.add(address + ":"
              + (port != null && port.length() != 0 ? port : "8080"));
        }
      }
    }
  }

  /**
   * Build structure handler.
   */
  public static class BuildHandler {
    protected boolean finished = false;
    protected int build;
    protected String custom;

    /*
     * Build type.
     */
    private class Type implements OptionMapper, OptionMapper.FinishMapper {
      @Override
      public void emit(final StringBuilder flags,
          final List<String> commandline, final String value) {
        if (value != null && value.length() != 0) {
          BuildHandler.this.build = Integer.parseInt(value);
        }
      }

      @Override
      public void finish(final StringBuilder flags,
          final List<String> commandline) {
        BuildHandler.this.finish(flags, commandline);
      }
    }

    /*
     * Custom build
     */
    private class Custom implements OptionMapper, OptionMapper.FinishMapper {
      @Override
      public void emit(final StringBuilder flags,
          final List<String> commandline, final String value) {
        if (value != null && value.length() != 0) {
          BuildHandler.this.custom = value;
        }
      }

      @Override
      public void finish(final StringBuilder flags,
          final List<String> commandline) {
        BuildHandler.this.finish(flags, commandline);
      }
    }

    /**
     * Get type mapper
     * 
     * @return the type mapper
     */
    public OptionMapper getTypeMapper() {
      return new Type();
    }

    /**
     * Get the custom build mapper
     * 
     * @return the custom build mapper
     */
    public OptionMapper getCustomMapper() {
      return new Custom();
    }

    /* Mapping to httrack build structure numbers. */
    private static final int mapping[] = new int[] { 0, 1, 2, 3, 4, 5, 100,
        101, 102, 103, 104, 105, 99, 199 };

    /*
     * Where we really emit the option
     */
    private void finish(final StringBuilder flags,
        final List<String> commandline) {
      if (!finished) {
        finished = true;
        if (build < mapping.length) {
          flags.append('N');
          flags.append(mapping[build]);
        } else if (build == mapping.length) {
          commandline.add("-N");
          commandline.add(custom);
        }
      }
    }
  }

  /**
   * Log settings handler.
   */
  public static class LogHandler {
    protected boolean finished = false;
    protected boolean enabled;
    protected int type;

    /*
     * Build type.
     */
    private class Type implements OptionMapper, OptionMapper.FinishMapper {
      @Override
      public void emit(final StringBuilder flags,
          final List<String> commandline, final String value) {
        if (value != null && value.length() != 0) {
          LogHandler.this.type = Integer.parseInt(value);
        }
      }

      @Override
      public void finish(final StringBuilder flags,
          final List<String> commandline) {
        LogHandler.this.finish(flags, commandline);
      }
    }

    /*
     * Custom build
     */
    private class Enabled implements OptionMapper, OptionMapper.FinishMapper {
      @Override
      public void emit(final StringBuilder flags,
          final List<String> commandline, final String value) {
        if (value != null && value.length() != 0) {
          LogHandler.this.enabled = "1".equals(value);
        }
      }

      @Override
      public void finish(final StringBuilder flags,
          final List<String> commandline) {
        LogHandler.this.finish(flags, commandline);
      }
    }

    /**
     * Get address mapper
     * 
     * @return the address mapper
     */
    public OptionMapper getTypeMapper() {
      return new Type();
    }

    /**
     * Get the port mapper
     * 
     * @return the port build mapper
     */
    public OptionMapper getEnabledMapper() {
      return new Enabled();
    }

    /*
     * Where we really emit the option
     */
    private void finish(final StringBuilder flags,
        final List<String> commandline) {
      if (!finished) {
        finished = true;
        if (enabled) {
          switch (type) {
          case 0:
            break;
          case 1:
            flags.append('z');
            break;
          case 2:
            flags.append('Z');
            break;
          }
        } else {
          flags.append('Q');
        }
      }
    }
  }

  /**
   * primary scan handler.
   */
  public static class PrimaryScanHandler {
    protected boolean finished = false;
    protected boolean htmlFirst;
    protected int type;

    /*
     * Type.
     */
    private class Type implements OptionMapper, OptionMapper.FinishMapper {
      @Override
      public void emit(final StringBuilder flags,
          final List<String> commandline, final String value) {
        if (value != null && value.length() != 0) {
          PrimaryScanHandler.this.type = Integer.parseInt(value);
        }
      }

      @Override
      public void finish(final StringBuilder flags,
          final List<String> commandline) {
        PrimaryScanHandler.this.finish(flags, commandline);
      }
    }

    /*
     * Html first ?
     */
    private class HtmlFirst implements OptionMapper, OptionMapper.FinishMapper {
      @Override
      public void emit(final StringBuilder flags,
          final List<String> commandline, final String value) {
        if (value != null && value.length() != 0) {
          PrimaryScanHandler.this.htmlFirst = "1".equals(value);
        }
      }

      @Override
      public void finish(final StringBuilder flags,
          final List<String> commandline) {
        PrimaryScanHandler.this.finish(flags, commandline);
      }
    }

    /**
     * Get address mapper
     * 
     * @return the address mapper
     */
    public OptionMapper getTypeMapper() {
      return new Type();
    }

    /**
     * Get the port mapper
     * 
     * @return the port build mapper
     */
    public OptionMapper getHtmlFirstMapper() {
      return new HtmlFirst();
    }

    /*
     * Where we really emit the option
     */
    private void finish(final StringBuilder flags,
        final List<String> commandline) {
      if (!finished) {
        finished = true;
        switch (type) {
        case 0:
        case 1:
        case 2:
          flags.append('p');
          flags.append(type);
          break;
        case 3:
          flags.append('p');
          if (!htmlFirst) {
            flags.append(type);
          } else {
            flags.append('7');
          }
          break;
        case 4:
          flags.append('p');
          flags.append('7');
          break;
        }
      }
    }
  }

  /**
   * Log settings handler.
   */
  public static class MimeTypesHandler {
    protected boolean finished = false;
    protected String ext;
    protected String mime;

    /*
     * Build type.
     */
    private class Ext implements OptionMapper, OptionMapper.FinishMapper {
      @Override
      public void emit(final StringBuilder flags,
          final List<String> commandline, final String value) {
        if (value != null && value.length() != 0) {
          MimeTypesHandler.this.ext = value.trim();
        }
      }

      @Override
      public void finish(final StringBuilder flags,
          final List<String> commandline) {
        MimeTypesHandler.this.finish(flags, commandline);
      }
    }

    /*
     * Custom build
     */
    private class Mime implements OptionMapper, OptionMapper.FinishMapper {
      @Override
      public void emit(final StringBuilder flags,
          final List<String> commandline, final String value) {
        if (value != null && value.length() != 0) {
          MimeTypesHandler.this.mime = value.trim();
        }
      }

      @Override
      public void finish(final StringBuilder flags,
          final List<String> commandline) {
        MimeTypesHandler.this.finish(flags, commandline);
      }
    }

    /**
     * Get ext mapper
     * 
     * @return the ext mapper
     */
    public OptionMapper getExtMapper() {
      return new Ext();
    }

    /**
     * Get mime mapper
     * 
     * @return the mime mapper
     */
    public OptionMapper getMimeMapper() {
      return new Mime();
    }

    /*
     * Where we really emit the option
     */
    private void finish(final StringBuilder flags,
        final List<String> commandline) {
      if (!finished) {
        finished = true;
        if (ext != null && mime != null && ext.length() != 0
            && mime.length() != 0) {
          commandline.add("%A");
          commandline.add(ext + "=" + mime);
        }
      }
    }
  }

  /**
   * Simple option (typically one character and an option)
   */
  public static class SimpleOption implements OptionMapper {
    protected final String option;

    public SimpleOption(final String option) {
      this.option = option;
    }

    @Override
    public void emit(final StringBuilder flags, final List<String> commandline,
        final String value) {
      if (value != null && value.length() != 0) {
        flags.append(option);
        flags.append(value);
      }
    }
  }

  /**
   * Argument option (<option> <value>)<br/>
   * Example: -F "user-agent"
   */
  public static class ArgumentOption implements OptionMapper {
    protected final String option;

    public ArgumentOption(final String option) {
      this.option = option;
    }

    @Override
    public void emit(final StringBuilder flags, final List<String> commandline,
        final String value) {
      if (value != null && value.length() != 0) {
        commandline.add(option);
        commandline.add(value);
      }
    }
  }

  /**
   * Option without any value, but with optional '0' disable flag. Example: -%k
   * and -%k0
   */
  public static class SimpleOption0 implements OptionMapper {
    protected final String option;

    public SimpleOption0(final String option) {
      this.option = option;
    }

    @Override
    public void emit(final StringBuilder flags, final List<String> commandline,
        final String value) {
      if (value != null && value.length() != 0) {
        flags.append(option);
        if ("0".equals(value)) {
          flags.append('0');
        }
      }
    }
  }

  /**
   * Option without any value. Example: -n or nothing
   */
  public static class SimpleOptionFlag implements OptionMapper {
    protected final String option;
    protected final boolean reverted;

    public SimpleOptionFlag(final String option, final boolean reverted) {
      this.option = option;
      this.reverted = reverted;
    }

    public SimpleOptionFlag(final String option) {
      this(option, false);
    }

    @Override
    public void emit(final StringBuilder flags, final List<String> commandline,
        final String value) {
      if (value != null && value.length() != 0) {
        if ((!reverted && "1".equals(value)) || (reverted && "0".equals(value))) {
          flags.append(option);
        }
      }
    }
  }

  /**
   * Option without any value.
   */
  public static class MultipleChoicesOption implements OptionMapper {
    protected final String[] choices;
    protected final boolean asFlag;

    public MultipleChoicesOption(final String[] choices, final boolean asFlag) {
      this.choices = choices;
      this.asFlag = asFlag;
    }

    @Override
    public void emit(final StringBuilder flags, final List<String> commandline,
        final String value) {
      if (value != null && value.length() != 0) {
        final int choiceId = Integer.parseInt(value);
        if (choiceId >= 0 && choiceId < choices.length) {
          final String choice = choices[choiceId];
          if (choice != null && choice.length() != 0) {
            if (asFlag) {
              flags.append(choice);
            } else {
              commandline.add(choice);
            }
          }
        }
      }
    }
  }

  /*
   * Build fieldsNameToId
   */
  private static void createFieldsNameToId() {
    // Create fieldsNameToId
    for (final Pair<Integer, String> field : fieldsSerializer) {
      final int id = field.first;
      final String fkey = field.second;
      if (fieldsNameToId.containsKey(fkey)) {
        throw new RuntimeException("unexpected internal error with " + fkey);
      }
      fieldsNameToId.put(fkey, id);
    }
  }

  /*
   * Build fieldsNameToMapper
   */
  private void createFieldsNameToMapper() {
    for (final Pair<String, OptionMapper> field : fieldsMapper) {
      final String id = field.first;
      final OptionMapper fkey = field.second;
      if (fieldsNameToMapper.containsKey(id)) {
        throw new RuntimeException("unexpected internal error with " + id);
      }
      fieldsNameToMapper.put(id, fkey);
    }
  }

  /*
   * Map special values, depending on the context.
   */
  private void setDynamicDefaults() {
    if (context != null) {
      map.put(fieldsNameToId.get("AcceptLanguage"),
          context.getString(R.string.language_iso_code) + ",*");
    }
  }

  /*
   * Fill the map with default values
   */
  private void initializeMap() {
    map.clear();
    for (final Pair<String, String> field : fieldsDefaults) {
      final Integer id = fieldsNameToId.get(field.first);
      if (id == null) {
        throw new RuntimeException("unexpecter error: unknown key "
            + field.first);
      }
      map.put(id, field.second);
    }
    setDynamicDefaults();
  }

  /**
   * Cleanup a space-separated string.
   * 
   * @param s
   *          The string
   * @return the cleaned up string
   */
  public static String cleanupString(final String s) {
    return s != null ? s.replaceAll("\\s+", " ").trim() : null;
  }

  /**
   * Is this space-separated string non empty ?
   * 
   * @param s
   *          The string
   * @return true if the string was not empty
   */
  public static boolean isStringNonEmpty(final String s) {
    final String cleaned = cleanupString(s);
    return cleaned != null && cleaned.length() != 0;
  }

  /**
   * Build a new options mapper.
   * 
   * @param context
   *          Attached context.
   */
  public OptionsMapper(final Context context) {
    setContext(context);

    // Create fieldsNameToMapper
    createFieldsNameToMapper();

    // Initialize default values for map
    initializeMap();
  }

  /**
   * Build a new options mapper.
   */
  public OptionsMapper() {
    this(null);
  }

  /**
   * Set the context.
   * 
   * @param context
   *          The context
   */
  public void setContext(final Context context) {
    this.context = context;
  }

  /**
   * Get a map value.
   * 
   * @param key
   *          The key
   * @return The value
   */
  public String getMap(final int key) {
    return map.get(key);
  }

  private String nonNullString(final String s) {
    return s != null ? s : "";
  }

  /**
   * Clear the map, wiping all keys/values, assigning default ones.
   */
  public void resetMap() {
    // Clear everything
    map.clear();

    // Initialize default values for map
    initializeMap();

    dirty = false;
  }

  /**
   * Set a map value.
   * 
   * @param key
   *          The key
   * @param value
   *          The value
   */
  public void setMap(final int key, final String value) {
    if (!dirty) {
      final String previous = map.get(key);
      dirty = previous != value
          && !nonNullString(previous).equals(nonNullString(value));
      if (dirty) {
        Log.d(getClass().getSimpleName(), "map set dirty: "
            + OptionsMapper.fieldsNameToId.get(key) + "=" + value + " (was "
            + previous + ")");
      }
    }
    map.put(key, value);
  }

  /**
   * Return the underlying map size.
   * 
   * @return the underlying map size
   */
  public int size() {
    return map.size();
  }

  /**
   * Get URLs
   * 
   * @return the URLs
   */
  protected String getProjectUrl() {
    return cleanupString(getMap(R.id.fieldWebsiteURLs));
  }

  /**
   * Get the current project name
   * 
   * @return The current project name
   */
  public String getProjectName() {
    return cleanupString(getMap(R.id.fieldProjectName));
  }

  /*
   * winprofile.ini encoding. the encoding is a bit lame, but is compatible with
   * WinHTTrack format.
   */
  private static String profileEncode(final String s) {
    final StringBuilder builder = new StringBuilder();
    for (int i = 0; i < s.length(); i++) {
      final char c = s.charAt(i);
      if (c == '%') {
        builder.append("%%");
      } else if (c < 32) {
        builder.append('%');
        builder.append(String.format("%02x", (int) c));
      } else {
        builder.append(c);
      }
    }
    return builder.toString();
  }

  /*
   * winprofile.ini decoding. the encoding is a bit lame, but is compatible with
   * WinHTTrack format.
   */
  private static String profileDecode(final String s) {
    final StringBuilder builder = new StringBuilder();
    for (int i = 0; i < s.length(); i++) {
      final char c = s.charAt(i);
      if (c == '%' && i + 1 < s.length()) {
        final char d = s.charAt(i + 1);
        if (d == '%') {
          i++;
          builder.append('%');
        } else if (i + 2 < s.length()) {
          try {
            final int code = Integer.parseInt(s.substring(i + 1, i + 3), 16);
            i += 2;
            builder.append((char) code);
          } catch (final NumberFormatException nfe) {

          }
        }
      } else {
        builder.append(c);
      }
    }
    return builder.toString();
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
  public static void unserialize(final File profile,
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
          final String value = OptionsMapper.profileDecode(line
              .substring(sep + 1));
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
   * Is the map dirty ?
   * 
   * @return true if the map is dirty
   */
  public boolean isDirty() {
    return dirty;
  }

  /**
   * Serialize settings on disk.
   * 
   * @param profile
   *          The profile file.
   * @throws UnsupportedEncodingException
   *           Upon encoding error
   * @throws IOException
   *           Upon I/O error.
   */
  public void serialize(final File profile)
      throws UnsupportedEncodingException, IOException {
    final FileWriter writer = new FileWriter(profile);
    final BufferedWriter lwriter = new BufferedWriter(writer);
    try {
      for (final Pair<Integer, String> field : OptionsMapper.fieldsSerializer) {
        final String value = getMap(field.first);
        final String key = field.second;
        lwriter.write(key);
        lwriter.write("=");
        if (value != null) {
          lwriter.write(OptionsMapper.profileEncode(value));
        }
        lwriter.write("\n");
      }
      lwriter.close();
      if (dirty) {
        dirty = false;
        Log.d(getClass().getSimpleName(),
            "map set clean: serialize to file (sync'ed)");
      }
    } finally {
      writer.close();
    }
  }

  /**
   * Serialize the map
   * 
   * @return a Parcelable object
   */
  public Parcelable serialize() {
    return map;
  }

  /**
   * Unserialize profile from disk.
   * 
   * @throws IOException
   *           Upon I/O error.
   */
  protected void unserialize(final File profile) throws IOException {
    unserialize(profile, map);
    if (dirty) {
      dirty = false;
      Log.d(getClass().getSimpleName(),
          "map set clean: unserialize from file (sync'ed)");
    }
  }

  /**
   * Unserialize object.
   * 
   * @param object
   *          the Parcelable object
   */
  public void unserialize(final Parcelable object) {
    map.unserialize(object);
  }

  /**
   * Build commandline arguments for the httrack engine, depending on current
   * defined settings.
   * 
   * @return The commandline argument(s)
   */
  public List<String> buildCommandline() {
    final StringBuilder flags = new StringBuilder("-");
    final List<String> list = new ArrayList<String>();

    // Map all options
    for (final Pair<Integer, String> field : OptionsMapper.fieldsSerializer) {
      final String value = getMap(field.first);
      final String key = field.second;
      if (value != null) {
        final OptionMapper map = fieldsNameToMapper.get(key);
        if (map != null) {
          map.emit(flags, list, value);
          // Log.v(getClass().getSimpleName(), "option mapped: " + key + "="
          // + value + " => " + flags);
        } else {
          Log.v(getClass().getSimpleName(), "option not mapped: " + key + "="
              + value);
        }
      }
    }

    // Call finish for special mappers
    for (final OptionMapper map : fieldsNameToMapper.values()) {
      if (map instanceof OptionMapper.FinishMapper) {
        final OptionMapper.FinishMapper finish = OptionMapper.FinishMapper.class
            .cast(map);
        finish.finish(flags, list);
        // Log.v(getClass().getSimpleName(), "finish: " +
        // map.getClass().getName() + " => " + flags);
      }
    }

    // Final args
    final List<String> args = new ArrayList<String>();

    // First option non-empty ?
    if (flags.length() > 1) {
      args.add(flags.toString());
      Log.v(getClass().getSimpleName(), "flags: " + flags);
    } else {
      Log.v(getClass().getSimpleName(), "no flags");
    }

    // Add all other args
    for (final String arg : list) {
      args.add(arg);
    }

    // Return final args
    return args;
  }
}
