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
import java.io.FileInputStream;
import java.io.FileOutputStream;
import java.io.FileReader;
import java.io.FileWriter;
import java.io.IOException;
import java.io.InputStream;
import java.io.PrintWriter;
import java.net.Inet6Address;
import java.net.InetAddress;
import java.net.NetworkInterface;
import java.net.SocketException;
import java.util.ArrayList;
import java.util.Enumeration;
import java.util.List;
import java.util.regex.Pattern;
import java.util.zip.ZipEntry;
import java.util.zip.ZipInputStream;

import android.app.Activity;
import android.app.AlertDialog;
import android.app.Notification;
import android.app.NotificationManager;
import android.app.PendingIntent;
import android.content.Context;
import android.content.Intent;
import android.content.SharedPreferences;
import android.content.pm.ApplicationInfo;
import android.content.pm.PackageInfo;
import android.content.pm.PackageManager.NameNotFoundException;
import android.content.res.Configuration;
import android.net.Uri;
import android.os.AsyncTask;
import android.os.Bundle;
import android.os.Environment;
import android.os.Handler;
import android.os.Parcelable;
import android.support.v4.app.Fragment;
import android.support.v4.app.FragmentActivity;
import android.support.v4.app.FragmentManager;
import android.support.v4.app.NotificationCompat;
import android.text.Editable;
import android.text.Html;
import android.text.TextUtils;
import android.text.TextWatcher;
import android.util.Log;
import android.view.Menu;
import android.view.MenuItem;
import android.view.View;
import android.view.ViewGroup.LayoutParams;
import android.view.animation.Animation;
import android.view.animation.AnimationUtils;
import android.widget.ArrayAdapter;
import android.widget.AutoCompleteTextView;
import android.widget.EditText;
import android.widget.LinearLayout;
import android.widget.ProgressBar;
import android.widget.RadioGroup;
import android.widget.TextView;
import android.widget.Toast;

import com.httrack.android.jni.HTTrackCallbacks;
import com.httrack.android.jni.HTTrackLib;
import com.httrack.android.jni.HTTrackStats;
import com.httrack.android.jni.HTTrackStats.Element;

public class HTTrackActivity extends FragmentActivity {
  // Page layouts
  protected static final int layouts[] = { R.layout.activity_startup,
      R.layout.activity_proj_name, R.layout.activity_proj_setup,
      R.layout.activity_mirror_progress, R.layout.activity_mirror_finished };

  // Special layout positions
  protected static final int LAYOUT_START = 0;
  protected static final int LAYOUT_PROJECT_NAME = 1;
  protected static final int LAYOUT_PROJECT_SETUP = 2;
  protected static final int LAYOUT_MIRROR_PROGRESS = 3;
  protected static final int LAYOUT_FINISHED = 4;

  // Preferences
  protected static final String PREFS_NAME = "HTTrackPreferences";
  protected static final String BASE_NAME = "BasePath";

  /*
   * Build identifiers. See
   * <http://developer.android.com/reference/android/os/Build
   * .VERSION_CODES.html>
   */
  private static class VERSION_CODES {
    protected static final int HONEYCOMB = 0x0000000b;
  };

  // Fields to restore/save state (Note: might be read-only fields)
  protected static final int fields[][] = { {},
      { R.id.fieldProjectName, R.id.fieldProjectCategory, R.id.fieldBasePath },
      { R.id.fieldWebsiteURLs, R.id.radioAction }, {}, { R.id.fieldDisplay } };

  // Options mapper, containing all options
  protected final OptionsMapper mapper = new OptionsMapper();

  // Activity identifier when using startActivityForResult()
  protected static final int ACTIVITY_OPTIONS = 0;
  protected static final int ACTIVITY_FILE_CHOOSER = 1;

  // Engine
  protected RunnerFragment runner = null;

  // Current pane id and context
  protected int pane_id = -1;

  // Handler to execute code in UI thread
  private final Handler handlerUI = new Handler();

  // Interrupt was requested
  protected boolean interruptRequested;

  // Warn spaces in project name
  protected static final int currentapiVersion = android.os.Build.VERSION.SDK_INT;
  protected boolean warnPreHoneycombSpaceIssue;
  protected boolean switchEmptyProjectName;

  // Widget data exchange helper
  private final WidgetDataExchange widgetDataExchange = new WidgetDataExchange(
      this);

  // Project settings
  protected String version;
  protected String versionFeatures;
  protected int versionCode;
  protected File projectPath;
  protected File rscPath;

  /* Get the root storage. */
  private File getExternalStorage() {
    final String state = Environment.getExternalStorageState();
    if (Environment.MEDIA_MOUNTED.equals(state)) {
      return Environment
          .getExternalStoragePublicDirectory(Environment.DIRECTORY_DOWNLOADS);
    } else {
      // Fallback.
      return getFilesDir();
    }
  }

  /* Get the default root external storage. */
  private File getDefaultHTTrackPath() {
    final File rootPath = getExternalStorage();
    final File httrackPath = new File(rootPath, "HTTrack");
    return new File(httrackPath, "Websites");
  }

  /*
   * (Re)Compute (external) storage pathes for downloaded websites.
   */
  private void computeStorageTarget() {
    final SharedPreferences settings = getSharedPreferences(PREFS_NAME, 0);
    final String base = settings.getString(BASE_NAME, null);
    if (base != null) {
      projectPath = new File(base);
    } else if (projectPath == null || !projectPath.exists()) {
      projectPath = getDefaultHTTrackPath();
    }

    // Change ?
    final View view = findViewById(R.id.fieldBasePath);
    if (view != null) {
      TextView.class.cast(view).setText(projectPath.getAbsolutePath());
    }
  }

  /*
   * Set the base path.
   */
  private void setBasePath(final String path) {
    final SharedPreferences settings = getSharedPreferences(PREFS_NAME, 0);
    final SharedPreferences.Editor editor = settings.edit();
    editor.putString(BASE_NAME, path);
    editor.commit();
    // Load it
    computeStorageTarget();
    refreshprojectNameSuggests();
  }

  /*
   * Refresh project name suggest.
   */
  private void refreshprojectNameSuggests() {
    final AutoCompleteTextView name = AutoCompleteTextView.class.cast(this
        .findViewById(R.id.fieldProjectName));
    if (name != null) {
      /* Setup name selection adapter. */
      final String[] names = getProjectNames();
      if (names != null) {
        Log.v(getClass().getSimpleName(), "project names: " + printArray(names));
        name.setAdapter(new ArrayAdapter<String>(this,
            android.R.layout.simple_dropdown_item_1line, names));
      }
    }
  }

  /*
   * Check if the external storage is available, and if not, display a warning
   * message.
   */
  private void warnIfExternalStorageUnsuitable() {
    String message;
    final String state = Environment.getExternalStorageState();
    if (Environment.MEDIA_MOUNTED.equals(state)) {
      final File root = getProjectRootFile();
      if (root == null) {
        message = getString(R.string.could_not_get_external_directory);
      } else if (!root.exists()) {
        message = getString(R.string.could_not_write_to) + " "
            + root.getAbsolutePath();
      } else {
        return;
      }
    } else {
      if (Environment.MEDIA_MOUNTED_READ_ONLY.equals(state)) {
        message = getString(R.string.read_only_storage_media);
      } else {
        message = getString(R.string.no_storage_media);
      }
    }
    showNotification(message + "\n"
        + getString(R.string.may_not_download_until_problem_fixed), true);
  }

  /*
   * Ensure the "Websites" directory exists.
   * 
   * Note: calls computeStorageTarget().
   */
  protected boolean ensureExternalStorage() {
    computeStorageTarget();
    final File root = getProjectRootFile();
    if (root != null && mkdirs(root)) {
      Log.d(getClass().getSimpleName(), "validated " + root.getAbsolutePath());
      return true;
    } else {
      Log.d(getClass().getSimpleName(),
          "could not create " + root.getAbsolutePath());
      return false;
    }
  }

  @Override
  protected void onCreate(final Bundle savedInstanceState) {
    super.onCreate(savedInstanceState);

    // Attempt to load the native library.
    try {
      // Initialize
      HTTrackLib.init();

      // Fetch httrack engine version
      version = HTTrackLib.getVersion();
      versionFeatures = HTTrackLib.getFeatures();
    } catch (final RuntimeException re) {
      // Woops, something is not right here.
      new AlertDialog.Builder(this).setTitle("Fatal Error")
          .setMessage(re.getMessage()).show();
      throw re;
    }

    // Android package version code
    try {
      final PackageInfo info = getPackageManager().getPackageInfo(
          getPackageName(), 0);
      versionCode = info.versionCode;
    } catch (final NameNotFoundException e) {
      new AlertDialog.Builder(this).setTitle("Fatal Error")
          .setMessage(e.getMessage()).show();
      throw new RuntimeException(e);
    }

    // Compute target directory on external storage
    ensureExternalStorage();

    // Check if an external storage is available
    warnIfExternalStorageUnsuitable();

    // Extract resources if necessary
    rscPath = buildResourceFile();

    // Ensure users can see us
    HTTrackActivity.setFileReadWrite(getProjectRootFile());

    // Clear map (useful to get dynamic fields)
    mapper.setContext(this);
    mapper.resetMap();

    // Go to first pane now
    setPane(0);

    // Load intent ? (example: shutdown phone, and power-it on during setup)
    final Bundle extras = getIntent().getExtras();
    if (extras != null) {
      restoreInstanceState(extras);
    }
  }

  /* Install/Update time. */
  private long installOrUpdateTime() {
    final ApplicationInfo appInfo = getApplicationInfo();
    final String appFile = appInfo.sourceDir;
    final long installed = new File(appFile).lastModified();
    return installed;
  }

  /**
   * Get the resource directory. Create it if necessary. Resources are created
   * in the dedicated cache, so that the files can be uninstalled upon
   * application removal.
   **/
  private File buildResourceFile() {
    final File cache = getExternalCacheDir();
    final File rscPath = new File(cache, "resources");
    final File stampFile = new File(rscPath, "resources.stamp");
    final long stamp = installOrUpdateTime();

    // Check timestamp of resources. If the applicate has been updated,
    // recreated cached resources.
    if (rscPath.exists()) {
      long diskStamp = 0;
      try {
        if (stampFile.exists()) {
          final FileReader reader = new FileReader(stampFile);
          final BufferedReader lreader = new BufferedReader(reader);
          try {
            diskStamp = Long.parseLong(lreader.readLine());
          } catch (final NumberFormatException nfe) {
            diskStamp = 0;
          }
          lreader.close();
          reader.close();
        }
      } catch (final IOException io) {
        diskStamp = 0;
      }
      // Different one: wipe and recreate
      if (stamp != diskStamp) {
        Log.i(getClass().getSimpleName(),
            "deleting old resources " + rscPath.getAbsolutePath()
                + " (app_stamp=" + stamp + " != disk_stamp=" + diskStamp + ")");
        CleanupActivity.deleteRecursively(rscPath);
      } else {
        Log.i(getClass().getSimpleName(),
            "keeping resources " + rscPath.getAbsolutePath()
                + " (app_stamp=disk_stamp=" + stamp + ")");
      }
    }

    // Recreate resources ?
    if (!rscPath.exists()) {
      Log.i(getClass().getSimpleName(),
          "creating resources " + rscPath.getAbsolutePath() + " with stamp "
              + stamp);
      if (HTTrackActivity.mkdirs(rscPath)) {
        long totalSize = 0;
        int totalFiles = 0;
        try {
          final InputStream zipStream = getResources().openRawResource(
              R.raw.resources);
          final ZipInputStream file = new ZipInputStream(zipStream);
          ZipEntry entry;
          while ((entry = file.getNextEntry()) != null) {
            final File dest = new File(rscPath.getAbsoluteFile() + "/"
                + entry.getName());
            if (entry.getName().endsWith("/")) {
              dest.mkdirs();
            } else {
              final FileOutputStream writer = new FileOutputStream(dest);
              final byte[] bytes = new byte[1024];
              int length;
              while ((length = file.read(bytes)) >= 0) {
                writer.write(bytes, 0, length);
                totalSize += length;
              }
              writer.close();
              totalFiles++;
              dest.setLastModified(entry.getTime());
            }
          }
          file.close();
          zipStream.close();
          Log.i(getClass().getSimpleName(),
              "created resources " + rscPath.getAbsolutePath() + " ("
                  + totalFiles + " files, " + totalSize + " bytes)");

          // Write stamp
          final FileWriter writer = new FileWriter(stampFile);
          final BufferedWriter lwriter = new BufferedWriter(writer);
          lwriter.write(Long.toString(stamp));
          lwriter.close();
          writer.close();

          // Little info
          showNotification(getString(R.string.info_recreated_resources));
        } catch (final IOException io) {
          Log.w(getClass().getSimpleName(), "could not create resources", io);
          CleanupActivity.deleteRecursively(rscPath);
          showNotification(getString(R.string.info_could_not_create_resources),
              true);
        }
      }
    }

    // Return resources path
    return rscPath;
  }

  /** Return a text resource as a string. **/
  private String getTextResource(final int id) {
    try {
      final InputStream is = getResources().openRawResource(id);
      final byte[] b = new byte[is.available()];
      is.read(b);
      return new String(b, "UTF-8");
    } catch (final IOException io) {
      return "resource not found";
    }
  }

  /**
   * Get the resource directory.
   * 
   * @return the resource directory
   */
  private File getResourceFile() {
    return rscPath;
  }

  /**
   * Get a map value.
   * 
   * @param key
   *          The key
   * @return The value
   */
  public String getMap(final int key) {
    return mapper.getMap(key);
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
    mapper.setMap(key, value);
  }

  /**
   * Trunk to mapper's buildCommandline()
   * 
   * @return
   */
  public synchronized List<String> buildCommandline() {
    return mapper.buildCommandline();
  }

  /**
   * Are there any projects yet ?
   * 
   * @return true if projects have been detected
   */
  protected boolean hasProjectNames() {
    final String[] names = getProjectNames();
    return names != null && names.length != 0;
  }

  /**
   * Get the root directory.
   * 
   * @return The root directory.
   */
  protected File getProjectRootFile() {
    return projectPath;
  }

  /**
   * Get the root index.html file.
   * 
   * @return The root index.html file.
   */
  protected File getProjectRootIndexFile() {
    final File target = getProjectRootFile();
    return new File(target, "index.html");
  }

  /**
   * A top index is present.
   * 
   * @return true if a top index.html is present
   */
  private boolean hasProjectRootIndexFile() {
    return getProjectRootIndexFile().exists();
  }

  /**
   * Return the already downloaded project names.
   * 
   * @return The list of project names.
   */
  protected String[] getProjectNames() {
    final List<String> list = new ArrayList<String>();
    final File dir = getProjectRootFile();
    if (dir.exists() && dir.isDirectory()) {
      for (final File item : dir.listFiles()) {
        if (item.isDirectory()) {
          final File profile = new File(new File(item, "hts-cache"),
              "winprofile.ini");
          if (profile.exists() && profile.isFile()) {
            list.add(item.getName());
          }
        }
      }
      return list.toArray(new String[] {});
    }
    return null;
  }

  /**
   * Get the destination directory for the current project.
   * 
   * @return The destination directory.
   */
  protected File getTargetFile() {
    final String name = mapper.getProjectName();
    if (name != null && name.length() != 0) {
      return new File(getProjectRootFile(), name);
    } else {
      return null;
    }
  }

  /**
   * Get the index.htm for the current project.
   * 
   * @return The destination directory.
   */
  protected File getTargetIndexFile() {
    final File target = getTargetFile();
    if (target != null) {
      return new File(target, "index.html");
    } else {
      return null;
    }
  }

  /**
   * Check whether an index.html file is present for the current project.
   * 
   * @return true if an index.html file is present for the current project.
   */
  protected boolean hasTargetIndexFile() {
    final File index = getTargetIndexFile();
    return index != null && index.exists();
  }

  /**
   * Get the hts-log.txt for the current project.
   * 
   * @return The destination directory.
   */
  protected File getTargetLogFile() {
    final File target = getTargetFile();
    if (target != null && target.exists()) {
      final File log = new File(target, "hts-log.txt");
      return log;
    } else {
      return null;
    }
  }

  /**
   * Check whether an hts-log.txt file is present for the current project.
   * 
   * @return true if an hts-log.txt file is present for the current project.
   */
  protected boolean hasTargetLogFile() {
    final File index = getTargetLogFile();
    return index != null && index.exists();
  }

  /**
   * Pretty-print a string array.
   * 
   * @param array
   *          The string array
   * @return The pretty-printed value
   */
  private static String printArray(final String[] array) {
    final StringBuilder builder = new StringBuilder();
    for (final String s : array) {
      if (builder.length() != 0) {
        builder.append(' ');
      }
      builder.append(s);
    }
    return builder.toString();
  }

  /**
   * Return the IPv6 address.
   * 
   * @return The ipv6 address, or @c null if no IPv6 connectivity is available.
   */
  protected static InetAddress getIPv6Address() {
    try {
      for (final Enumeration<NetworkInterface> interfaces = NetworkInterface
          .getNetworkInterfaces(); interfaces.hasMoreElements();) {
        final NetworkInterface iface = interfaces.nextElement();
        for (final Enumeration<InetAddress> addresses = iface
            .getInetAddresses(); addresses.hasMoreElements();) {
          final InetAddress address = addresses.nextElement();
          Log.d(HTTrackActivity.class.getSimpleName(), "seen interface: "
              + address.toString());
          if (address instanceof Inet6Address) {
            if (!address.isLoopbackAddress() && !address.isLinkLocalAddress()
                && !address.isSiteLocalAddress()
                && !address.isMulticastAddress()) {
              return address;
            }
          }
        }
      }
    } catch (final SocketException se) {
      Log.w(HTTrackActivity.class.getSimpleName(),
          "could not enumerate interfaces", se);
    }
    return null;
  }

  /**
   * is IPv6 available on this phone ?
   * 
   * @return true if IPv6 is available on this phone
   */
  protected static boolean isIPv6Enabled() {
    return getIPv6Address() != null;
  }

  /**
   * Emergency dump.
   */
  protected static void emergencyDump(final Throwable e) {
    try {
      final File dumpFile = new File(new File(
          Environment.getExternalStorageState(), "HTTrack"), "error.txt");
      final FileWriter writer = new FileWriter(dumpFile);
      final PrintWriter print = new PrintWriter(writer);
      e.printStackTrace(print);
      writer.close();
      HTTrackActivity.setFileReadWrite(dumpFile);
    } catch (final IOException io) {
    }
  }

  /**
   * Build the top index.
   * 
   * @return 1 upon success
   */
  protected synchronized int buildTopIndex() {
    // Build top index
    final File rsc = getResourceFile();
    if (rsc != null) {
      return HTTrackLib.buildTopIndex(getProjectRootFile(), rsc);
    } else {
      return 0;
    }
  }

  /**
   * The runner fragment class.<br />
   * Thanks to Alex Lockwood for the useful hints regarding fragments!
   */
  protected static class RunnerFragment extends Fragment {
    protected Runner runner;

    public RunnerFragment() {
    }

    @Override
    public void onCreate(final Bundle savedInstanceState) {
      super.onCreate(savedInstanceState);

      // Retain this fragment across configuration changes.
      setRetainInstance(true);
    }

    @Override
    public void onDestroy() {
      // Ensure the running job is killed when the fragment is disposed
      if (runner != null) {
        if (!runner.isEnded()) {
          if (runner.stopMirror(true)) {
            runner.sendAbortNotification();
          }
        }
      }
      super.onDestroy();
    }

    public void setParent(final HTTrackActivity parent) {
      if (runner != null) {
        throw new RuntimeException("can not attach twice");
      }
      // Create and execute the background task.
      runner = new Runner(parent);
      runner.execute();
    }

    // Attach activity
    @Override
    public void onAttach(final Activity activity) {
      super.onAttach(activity);
      final HTTrackActivity parent = HTTrackActivity.class.cast(activity);
      // We expect the runner not to be null at this point.
      if (runner != null) {
        runner.setParent(parent);
      } else {
        // We have to create the runner. Should not happen ?
        Throwable trace;
        try {
          throw new RuntimeException();
        } catch (final RuntimeException re) {
          trace = re;
        }
        Log.w(
            this.getClass().getSimpleName(),
            "empty fragment attached to an activity, creating the associated runner",
            trace);
        setParent(parent);
      }
    }

    // Detach activity
    @Override
    public void onDetach() {
      super.onDetach();
      runner.detach();
    }

    public boolean stopMirror(final boolean force) {
      return runner.stopMirror(force);
    }

    public HTTrackStats getLastStats() {
      return runner.getLastStats();
    }
  }

  /**
   * Engine thread runner.
   */
  protected static class Runner extends AsyncTask<Void, Integer, Void>
      implements HTTrackCallbacks {
    private final HTTrackLib engine = new HTTrackLib(this);
    final private StringBuilder str = new StringBuilder();
    private HTTrackActivity parent;
    private boolean mirrorRefresh;
    protected HTTrackStats lastStats;
    private volatile boolean ended;
    private volatile boolean interrupted;
    private volatile boolean interruptedHard;

    /**
     * Constructor.
     * 
     * @param parent
     *          the parent activity.
     */
    public Runner(final HTTrackActivity parent) {
      this.parent = parent;
    }

    /**
     * Set the parent activity.
     * 
     * @param parent
     *          the parent activity
     */
    public synchronized void setParent(final HTTrackActivity parent) {
      this.parent = parent;
    }

    /**
     * Detach a parent.
     */
    public synchronized void detach() {
      this.parent = null;
    }

    /** Trunk to parent's sendAbortNotification(). **/
    public synchronized void sendAbortNotification() {
      if (parent != null) {
        parent.sendAbortNotification();
      }
    }

    /* Background task. */
    @Override
    protected Void doInBackground(final Void... arg0) {
      try {
        runInternal();
      } catch (final RuntimeException e) {
        HTTrackActivity.emergencyDump(e);
        throw e;
      } finally {
        ended = true;
      }
      return null;
    }

    protected void runInternal() {
      final File target = parent.getTargetFile();
      final List<String> args = new ArrayList<String>();

      // Program name
      args.add("httrack");

      // If IPv6 is not available, do not use it.
      if (!isIPv6Enabled()) {
        args.add("-@i4");
      }

      // Target
      args.add("-O");
      args.add(target.getAbsolutePath());

      // Get args from mapper
      for (final String cmd : parent.buildCommandline()) {
        args.add(cmd);
      }

      // Final args array
      final String[] cargs = args.toArray(new String[] {});
      Log.v(getClass().getSimpleName(),
          "starting engine: " + HTTrackActivity.printArray(cargs));

      // Rock'in!
      String message = null;
      try {
        // Validate path
        if (!HTTrackActivity.mkdirs(target)) {
          throw new IOException("Unable to create " + target.getAbsolutePath());
        }
        HTTrackActivity.setFileReadWrite(target);

        // Serialize settings
        parent.serialize();

        // Run engine
        final int code = engine.main(cargs);

        // Result
        if (code == 0) {
          if (interrupted) {
            message = "<b>Interrupted</b>! (" + lastStats.errorsCount
                + " errors)";
          } else if (lastStats.errorsCount == 0) {
            message = "<b>Success</b>!";
          } else if (lastStats.filesWritten != 0) {
            message = "<b>Success</b>! (" + lastStats.errorsCount + " errors)";
          } else {
            message = "<b>Failed</b>! (" + lastStats.errorsCount
                + " errors, no files written)";
          }
          message += "<br /><br />Mirror copied in <i>"
              + target.getAbsolutePath() + "</i>";
        } else {
          message = "<b>Error</b> (<i>code " + code + "</i>)";
        }

        // Build top index
        synchronized (this) {
          if (parent != null) {
            parent.buildTopIndex();
          }
        }
      } catch (final IOException io) {
        message = io.getMessage();
      }

      // Ensure we switch to the final pane
      final String displayMessage = (parent != null ? parent
          .getString(R.string.mirror_finished) + ": " : "")
          + message;
      final long errorsCount = lastStats != null ? lastStats.errorsCount : 0;
      if (parent != null) {
        parent.handlerUI.post(new Runnable() {
          @Override
          public synchronized void run() {
            if (parent != null) {
              // Final pane
              parent.setPane(LAYOUT_FINISHED);

              // Fancy result message
              if (displayMessage != null) {
                final View view = parent.findViewById(R.id.fieldDisplay);
                if (view != null) {
                  TextView.class.cast(view).setText(
                      Html.fromHtml(displayMessage));
                }
              }
              if (errorsCount != 0) {
                final View view = parent.findViewById(R.id.buttonLogs);
                if (view != null) {
                  final Animation shake = AnimationUtils.loadAnimation(parent,
                      R.anim.scale);
                  view.startAnimation(shake);
                }
              }
            }
          }
        });
      }
    }

    /**
     * Stop the mirror.
     */
    public boolean stopMirror(final boolean force) {
      // Set interrupted flags
      interrupted = true;
      if (force) {
        interruptedHard = true;
      }
      // Stop engine
      final boolean stopSent = engine.stop(force);
      // If not yet stopped, mark as dirty
      // ("Continue an interrupted mirror ...")
      try {
        synchronized (this) {
          if (parent != null) {
            if (stopSent) {
              parent.setInterruptedProfile(true);
            } else {
              parent.setInterruptedProfile(false);
            }
          } else {
            throw new IOException("parent has been detached");
          }
        }
      } catch (final IOException io) {
        Log.w(getClass().getSimpleName(), "could not lock file", io);
      }
      return stopSent;
    }

    /**
     * Has the mirror been interrupted ?
     */
    public boolean isInterrupted() {
      return interrupted;
    }

    /**
     * Has the mirror stopped ?
     */
    public boolean isEnded() {
      return ended;
    }

    @Override
    public void onRefresh(HTTrackStats stats) {
      // fake first refresh for cosmetic reasons.
      if (stats == null) {
        if (mirrorRefresh) {
          return;
        }
        mirrorRefresh = true;
        stats = new HTTrackStats();
      } else {
        synchronized (this) {
          lastStats = stats;
        }
      }

      // Do not refresh GUI if stopped
      if (interruptedHard) {
        return;
      }

      synchronized (this) {
        if (parent == null) {
          return;
        }

        // build stats infos
        final String sep = " • ";
        str.setLength(0);
        str.append("<b>" + parent.getString(R.string.bytes_saved) + "</b>: ");
        str.append(stats.bytesWritten);
        str.append(sep);
        str.append("<b>" + parent.getString(R.string.links_scanned) + "</b>: ");
        str.append(stats.linksScanned);
        str.append("/");
        str.append(stats.linksTotal);
        str.append(" (+");
        str.append(stats.linksBackground);
        str.append(")<br />");
        /* */
        str.append("<b>" + parent.getString(R.string.time) + "</b>: ");
        str.append(stats.elapsedTime);
        str.append(sep);
        str.append("<b>" + parent.getString(R.string.files_written) + "</b>: ");
        str.append(stats.filesWritten);
        str.append(" (+");
        str.append(stats.filesWrittenBackground);
        str.append(")<br />");
        /* */
        str.append("<b>" + parent.getString(R.string.transfer_rate) + "</b>: ");
        str.append(stats.transferRate);
        str.append(" (");
        str.append(stats.totalTransferRate);
        str.append(")");
        str.append(sep);
        str.append("<b>" + parent.getString(R.string.files_updated) + "</b>: ");
        str.append(stats.filesUpdated);
        str.append("<br />");
        /* */
        str.append("<b>" + parent.getString(R.string.active_connections)
            + "</b>: ");
        str.append(stats.socketsCount);
        str.append(sep);
        str.append("<b>" + parent.getString(R.string.errors) + "</b>:");
        str.append(stats.errorsCount);
        /* */
        if (stats.elements != null && stats.elements.length != 0) {
          str.append("<br />");
          str.append("<i><br />");
          int maxElts = 32; // limit the number of displayed items
          for (final Element element : stats.elements) {
            if (element == null || element.address == null
                || element.filename == null) {
              continue;
            }
            if (--maxElts == 0) {
              break;
            }

            // URL
            str.append(element.address);
            str.append(element.path);
            str.append(" → ");

            // state
            switch (element.state) {
            case Element.STATE_CONNECTING:
              str.append(parent.getString(R.string.connect));
              break;
            case Element.STATE_DNS:
              str.append("dns");
              break;
            case Element.STATE_FTP:
              str.append("ftp");
              break;
            case Element.STATE_READY:
              str.append("<b>");
              str.append(parent.getString(R.string.ready));
              str.append("</b>");
              break;
            case Element.STATE_RECEIVE:
              if (element.totalSize > 0) {
                final long completion = (100 * element.size + element.totalSize / 2)
                    / element.totalSize;
                str.append(completion);
                str.append("%");
              } else {
                str.append(element.size);
                str.append("B");
              }
              break;
            default:
              str.append("???");
              break;
            }
            str.append("<br />");
          }
          str.append("</i>");
        }
        final String message = str.toString();
        // Post refresh.
        if (parent != null) {
          parent.handlerUI.post(new Runnable() {
            @Override
            public synchronized void run() {
              if (parent != null) {
                parent.setProgressLines(message);
              }
            }
          });
        }
      }
    }

    /**
     * Get last statistics
     * 
     * @return last statistics (or @c null if none)
     */
    public synchronized HTTrackStats getLastStats() {
      return lastStats;
    }
  }

  /**
   * Get the profile target file for the current project.
   * 
   * @return The profile target file.
   */
  protected File getProfileFile() {
    final File target = getTargetFile();
    if (target != null) {
      return getProfileFile(target);
    } else {
      return null;
    }
  }

  /**
   * Get the profile cache file for the current project.
   * 
   * @return The profile cache file.
   */
  protected File getCacheFile() {
    final File target = getTargetFile();
    if (target != null) {
      return getCacheFile(target);
    } else {
      return null;
    }
  }

  /**
   * Is there an existing profile yet ?
   * 
   * @return true if there is a winprofile.ini file
   */
  protected boolean hasProfileFile() {
    final File profile = getProfileFile();
    return profile != null && profile.exists();
  }

  /**
   * Is there an existing cache yet ?
   * 
   * @return true if there is a new.zip cache file
   */
  protected boolean hasCacheFile() {
    final File profile = getCacheFile();
    return profile != null && profile.exists();
  }

  /**
   * Interrupted profile ?
   * 
   * @return true if the mirror was interrupted
   */
  protected boolean isInterruptedProfile() {
    final File target = getTargetFile();
    if (target != null) {
      final File cache = new File(target, "hts-cache");
      return new File(target, "hts-in_progress.lock").exists()
          || new File(cache, "interrupted.lock").exists();
    } else {
      return false;
    }
  }

  /**
   * Set the "interrupted" flag.
   * 
   * @param interrupted
   *          Interrupted mirror ?
   * @throws IOException
   *           Upon I/O error.
   */
  protected synchronized void setInterruptedProfile(final boolean interrupted)
      throws IOException {
    final File target = getTargetFile();
    final File cache = new File(target, "hts-cache");
    final File lock = new File(cache, "interrupted.lock");
    if (interrupted) {
      final FileWriter wr = new FileWriter(lock);
      wr.close();
    } else {
      lock.delete();
    }
  }

  /**
   * Get the profile target file for a given project.
   * 
   * @return The profile target file.
   * @param target
   *          The project directory
   * @return The profile file
   */
  protected static File getProfileFile(final File target) {
    if (target != null) {
      return new File(new File(target, "hts-cache"), "winprofile.ini");
    } else {
      return null;
    }
  }

  /**
   * Get the cache file for a given project.
   * 
   * @return The profile target file.
   * @param target
   *          The project directory
   * @return The cache file
   */
  protected static File getCacheFile(final File target) {
    if (target != null) {
      return new File(new File(target, "hts-cache"), "new.zip");
    } else {
      return null;
    }
  }

  /** Make directorie(s) if necessary. **/
  private static boolean mkdirs(final File target) {
    return target.mkdirs() || target.isDirectory();
  }

  /** make the given file readable/writabe. **/
  private static boolean setFileReadWrite(final File target) {
    // return target.setReadable(true) && target.setWritable(true);
    return true;
  }

  /**
   * Serialize current profile to disk.
   * 
   * @throws IOException
   *           Upon I/O error.
   */
  protected synchronized void serialize() throws IOException {
    final File target = getTargetFile();
    final File profile = getProfileFile();
    if (target == null || profile == null) {
      throw new IOException("No project defined yet");
    }
    final File cache = profile.getParentFile();

    // Validate path
    if (!HTTrackActivity.mkdirs(target)) {
      throw new IOException("Unable to create " + target.getAbsolutePath());
    }
    // Create cache for winprofile.ini
    else if (!HTTrackActivity.mkdirs(cache)) {
      throw new IOException("Unable to create " + cache);
    }
    HTTrackActivity.setFileReadWrite(target);
    HTTrackActivity.setFileReadWrite(cache);

    // Write settings
    mapper.serialize(profile);
    HTTrackActivity.setFileReadWrite(profile);
  }

  /**
   * Unserialize profile from disk.
   * 
   * @throws IOException
   *           Upon I/O error.
   */
  protected void unserialize() throws IOException {
    final File profile = getProfileFile();
    mapper.unserialize(profile);
  }

  /**
   * Start the runner
   */
  protected synchronized void startRunner() {
    final String id = "runner_task";

    // First attempt to reclaim a running one (orientation change)
    if (runner == null) {
      final FragmentManager fm = getSupportFragmentManager();
      runner = (RunnerFragment) fm.findFragmentByTag(id);
    }
    // Then, create one if necessary
    if (runner == null) {
      final FragmentManager fm = getSupportFragmentManager();
      runner = new RunnerFragment();
      runner.setParent(this);
      fm.beginTransaction().add(runner, id).commit();
    }
  }

  /**
   * We just entered in a new pane.
   */
  protected void onEnterNewPane() {
    switch (layouts[pane_id]) {
    case R.layout.activity_startup:
      final TextView text = TextView.class.cast(this
          .findViewById(R.id.fieldDisplay));
      final TextView textDebug = TextView.class.cast(this
          .findViewById(R.id.fieldDebug));

      // Welcome message.
      final String html = getString(R.string.welcome_message)
          .replace("\n-", "\n•").replace("\n", "<br />")
          .replace("HTTrack Website Copier", "<b>HTTrack Website Copier</b>");
      text.setText(Html.fromHtml(html));

      // Debugging and information.
      final StringBuilder str = new StringBuilder();
      str.append("<i>");
      if (version != null) {
        str.append("<br />Version: ");
        // Library version
        str.append(version);
        str.append(versionFeatures);
        // Android version
        str.append(" (Android version ");
        str.append(versionCode);
        str.append(")");
      }
      // str.append(" • Path: ");
      // str.append(getProjectRootFile().getAbsolutePath());
      str.append(" • IPv6: ");
      final InetAddress addrV6 = getIPv6Address();
      str.append(addrV6 != null ? ("YES (" + addrV6.getHostAddress() + ")")
          : "NO");
      str.append("</i>");
      textDebug.setText(Html.fromHtml(str.toString()));

      // Enable or disable browse & cleanup button depending on existing
      // project(s)
      View.class.cast(this.findViewById(R.id.buttonClear)).setEnabled(
          hasProjectNames());
      View.class.cast(this.findViewById(R.id.buttonBrowseAll)).setEnabled(
          hasProjectRootIndexFile());

      break;
    case R.layout.activity_proj_name:
      // Refresh suggest
      refreshprojectNameSuggests();

      /* Base path */
      TextView.class.cast(findViewById(R.id.fieldBasePath)).setText(
          getProjectRootFile().getAbsolutePath());

      // "Next" button is disabled if no project name is defined
      switchEmptyProjectName = !OptionsMapper.isStringNonEmpty(mapper
          .getProjectName());
      View.class.cast(findViewById(R.id.buttonNext)).setEnabled(
          !switchEmptyProjectName);

      /*
       * Prior to Honeycomb (TODO FIXME: check that), the android browser is
       * unable to browse local file:// pages embedding spaces (%20 or +)
       * Therefore, warn the user.
       */
      warnPreHoneycombSpaceIssue = currentapiVersion < VERSION_CODES.HONEYCOMB;

      /* Add text watcher for the "Next" button. */
      final AutoCompleteTextView name = AutoCompleteTextView.class.cast(this
          .findViewById(R.id.fieldProjectName));
      name.addTextChangedListener(new TextWatcher() {
        @Override
        public void onTextChanged(final CharSequence s, final int start,
            final int before, final int count) {
          // Warn when seeing space
          if (warnPreHoneycombSpaceIssue) {
            for (int i = start; i < start + count; i++) {
              if (s.charAt(i) == ' ') {
                showNotification(getString(R.string.warning_space_in_filename),
                    true);
                warnPreHoneycombSpaceIssue = false;
                break;
              }
            }
          }
        }

        @Override
        public void afterTextChanged(final Editable s) {
          // Enable/disable next button
          boolean empty = true;
          for (int i = 0; i < s.length(); i++) {
            if (Character.isLetterOrDigit(s.charAt(i))) {
              empty = false;
              break;
            }
          }
          if (empty != switchEmptyProjectName) {
            switchEmptyProjectName = empty;
            View.class.cast(findViewById(R.id.buttonNext)).setEnabled(!empty);
          }
        }

        // NOOP
        @Override
        public void beforeTextChanged(final CharSequence s, final int start,
            final int count, final int after) {
        }
      });
      break;
    case R.layout.activity_proj_setup:
      // Existing cache ?
      final boolean hasProfile = hasCacheFile();
      View.class.cast(this.findViewById(R.id.radioAction)).setEnabled(
          hasProfile);
      View.class.cast(this.findViewById(R.id.radioAction)).setVisibility(
          hasProfile ? View.VISIBLE : View.GONE);
      if (hasProfile) {
        // Update is the default unless something was broken
        RadioGroup.class.cast(this.findViewById(R.id.radioAction)).check(
            isInterruptedProfile() ? R.id.radioItemContinue
                : R.id.radioItemUpdate);
      } else {
        // Reset
        RadioGroup.class.cast(this.findViewById(R.id.radioAction)).check(-1);
      }
      break;
    case R.layout.activity_mirror_progress:
      startRunner();
      if (runner != null) {
        ProgressBar.class.cast(findViewById(R.id.progressMirror))
            .setVisibility(View.VISIBLE);
      }
      break;
    case R.layout.activity_mirror_finished:
      // Ensure the engine has stopped running
      if (runner != null) {
        runner.stopMirror(true);
      }

      // Enable browse button if index.html exists
      final boolean hasIndex = hasTargetIndexFile();
      View.class.cast(this.findViewById(R.id.buttonBrowse))
          .setEnabled(hasIndex);
      if (!hasIndex) {
        final File index = getTargetIndexFile();
        Log.d(getClass().getSimpleName(), "no index found ("
            + (index != null ? index.getAbsolutePath() : "unknown location")
            + ")");
        final String warning = getString(R.string.no_index_html_in_xx).replace(
            "%s", getTargetFile().getPath());
        showNotification(warning);
      }

      // Enable logs if present
      final boolean hasLog = hasTargetLogFile();
      View.class.cast(this.findViewById(R.id.buttonLogs)).setEnabled(hasLog);
      if (!hasLog) {
        final File log = getTargetLogFile();
        Log.d(getClass().getSimpleName(),
            "no log found ("
                + (log != null ? log.getAbsolutePath() : "unknown location")
                + ")");
      }

      // Final stats
      if (runner != null) {
        final HTTrackStats lastStats = runner.getLastStats();
        if (lastStats != null && lastStats.errorsCount != 0) {
          // TODO
        }
      }
      break;
    }
  }

  /**
   * Exit button.
   */
  protected void onFinish() {
    // FIXME TODO CRASHES!
    // runOnUiThread(new Runnable() {
    // public void run() {
    // finish();
    // }
    // });
  }

  /**
   * Get a specific field text.
   * 
   * @param id
   *          The field ID.
   * @return the associated text
   */
  private String getFieldText(final int id) {
    return widgetDataExchange.getFieldText(id);
  }

  /**
   * Set a specific field text value.
   * 
   * @param id
   *          The field ID.
   * @param value
   *          the associated text
   */
  private void setFieldText(final int id, final String value) {
    widgetDataExchange.setFieldText(id, value);
  }

  /*
   * Set the "progress" layout lines.
   */
  private void setProgressLines(final String linesBuffer) {
    final String[] lines = linesBuffer.split(Pattern.quote("<br />"));
    final LinearLayout layout = LinearLayout.class
        .cast(findViewById(R.id.layout));
    final int currSize = layout.getChildCount();
    for (int i = 0; i < lines.length; i++) {
      TextView text;
      if (i < currSize) {
        text = TextView.class.cast(layout.getChildAt(i));
        text.setWidth(LayoutParams.FILL_PARENT);
        text.setHeight(LayoutParams.WRAP_CONTENT);
        text.setEllipsize(TextUtils.TruncateAt.MIDDLE);
        text.setSingleLine();
      } else {
        text = new TextView(this);
        layout.addView(text);
      }
      final String line = lines[i];
      text.setText(Html.fromHtml(line));
    }
    if (lines.length < currSize) {
      layout.removeViews(lines.length, currSize - lines.length);
    }
  }

  /**
   * Validate the current pane
   * 
   * @return true if the current pane is valid
   */
  protected boolean validatePane() {
    switch (pane_id) {
    case LAYOUT_START:
      // Warn if no sdcard
      warnIfExternalStorageUnsuitable();
      break;
    case LAYOUT_PROJECT_NAME:
      // Recompute storage target if necessary
      if (!ensureExternalStorage()) {
        warnIfExternalStorageUnsuitable();
        return false;
      }
      // Continue if we could have a place to write to (even if we warned)

      // Check project name
      final String name = getFieldText(R.id.fieldProjectName);
      if (OptionsMapper.isStringNonEmpty(name)) {
        // Changed name ?
        final String prevName = getMap(R.id.fieldProjectName);
        if (prevName == null || !prevName.equals(name)) {
          // We need to put immediately the name in the map to be able to
          // unserialize.
          try {
            mapper.resetMap();
            setMap(R.id.fieldProjectName, name);
            unserialize();
          } catch (final IOException e) {
            // Ignore (if not found)
          }
        }
        return true;
      }
      return false;
    case LAYOUT_PROJECT_SETUP:
      return OptionsMapper
          .isStringNonEmpty(getFieldText(R.id.fieldWebsiteURLs));
    }
    return true;
  }

  /**
   * Validate the current pane with visual effects on error. Thanks to Sushant
   * for the idea.
   * 
   * @return true if the current pane is valid
   */
  protected boolean validatePaneWithEffects(final boolean next) {
    final boolean validated = validatePane();
    if (!validated) {
      final Animation shake = AnimationUtils.loadAnimation(this, R.anim.shake);
      findViewById(next ? R.id.buttonNext : R.id.buttonPrevious)
          .startAnimation(shake);
    }
    return validated;
  }

  /* Save current pane fieds into map. */
  private void savePaneFields() {
    if (pane_id != -1) {
      for (final int id : fields[pane_id]) {
        final String value = getFieldText(id);
        setMap(id, value);
      }
    }
  }

  /* Load pane fields from map. */
  private void loadPaneFields() {
    // Entering a new pane: restore data
    for (final int id : fields[pane_id]) {
      final String value = getMap(id);
      setFieldText(id, value);
    }
  }

  private void setPane(final int position) {
    if (pane_id != position) {
      // Leaving a pane: save data
      savePaneFields();

      // Switch pane
      pane_id = position;
      setContentView(layouts[pane_id]);

      // Entering a new pane: restore data
      loadPaneFields();

      // Post-actions
      onEnterNewPane();
    }
  }

  @Override
  public boolean onCreateOptionsMenu(final Menu menu) {
    // Inflate the menu; this adds items to the action bar if it is present.
    if (pane_id != -1) {
      getMenuInflater().inflate(R.menu.menu, menu);
    }
    return true;
  }

  /**
   * "Next"
   */
  public void onClickNext(final View view) {
    if (pane_id < layouts.length) {
      if (validatePaneWithEffects(true)) {
        setPane(pane_id + 1);
      }
    } else {
      onFinish();
    }
  }

  /**
   * "Previous"
   */
  public void onClickPrevious(final View view) {
    if (pane_id > 0) {
      setPane(pane_id - 1);
    }
  }

  /**
   * Change base path
   */
  public void onClickBasePath(final View view) {
    // Start new activity
    final Intent intent = new Intent(this, FileChooserActivity.class);
    fillExtra(intent);
    intent.putExtra("com.httrack.android.defaultHTTrackPath",
        getDefaultHTTrackPath());
    startActivityForResult(intent, ACTIVITY_FILE_CHOOSER);
  }

  /**
   * "Interrupt" or "Stop"
   */
  public void onClickStop(final View view) {
    if (runner != null) {
      // Soft interrupt
      if (!interruptRequested) {
        runner.stopMirror(false);
        interruptRequested = true;
        final TextView text = (TextView) this.findViewById(R.id.buttonStop);
        // Change text to "Stop"
        text.setText(getString(R.string.cancel));
        // Notice
        showNotification(getString(R.string.finishing_pending_transfers), true);
      }
      // Hard interrupt
      else {
        runner.stopMirror(true);
      }
    }
  }

  /**
   * "Options"
   */
  public void onClickOptions(final View view) {
    // First save current pane settings
    savePaneFields();

    // Then start new activity
    final Intent intent = new Intent(this, OptionsActivity.class);
    fillExtra(intent);
    intent.putExtra("com.httrack.android.map", mapper.serialize());
    Log.d(getClass().getSimpleName(), "map size: " + mapper.size());
    startActivityForResult(intent, ACTIVITY_OPTIONS);
  }

  /** Restore a previously saved map context. **/
  private void loadParcelable(final Parcelable data) {
    // Load modified map
    mapper.unserialize(data);
    Log.d(getClass().getSimpleName(), "received map size: " + mapper.size());

    // Load possibly modified field(s)
    loadPaneFields();
  }

  @Override
  protected void onActivityResult(final int requestCode, final int resultCode,
      final Intent data) {
    super.onActivityResult(requestCode, resultCode, data);
    switch (requestCode) {
    case ACTIVITY_OPTIONS:
      if (resultCode == Activity.RESULT_OK) {
        // Load modified map
        loadParcelable(data.getParcelableExtra("com.httrack.android.map"));
      }
      break;
    case ACTIVITY_FILE_CHOOSER:
      if (resultCode == Activity.RESULT_OK) {
        // Load modified map
        final String path = data.getStringExtra("com.httrack.android.rootFile");
        setBasePath(path);
      }
      break;
    }
  }

  /**
   * "Show Logs"
   */
  public void onShowLogs(final View view) {
    final File log = getTargetLogFile();
    if (log.exists()) {
      FileInputStream rd;
      try {
        rd = new FileInputStream(log);
        final byte[] data = new byte[(int) log.length()];
        rd.read(data);
        rd.close();
        final String logs = new String(data, "UTF-8");
        new AlertDialog.Builder(this).setTitle("Logs").setMessage(logs).show();
      } catch (final IOException e) {
      }
    }
  }

  /** Browser a specific index. **/
  public static Intent getBrowseIntent(final File index) {
    if (index != null && index.exists()) {
      final Intent intent = new Intent();
      intent.setAction(android.content.Intent.ACTION_VIEW);
      // Note: won't work on certain Android releases if the project name has
      // spaces :(
      final Uri uri = Uri.fromFile(index);
      // Without the MIME, Android tend to crash with a NPE (!)
      intent.setDataAndType(uri, "text/html");
      return intent;
    } else {
      return null;
    }
  }

  /** Browser a specific web page. **/
  private void browse(final Uri uri) {
    final Intent intent = new Intent();
    intent.setAction(android.content.Intent.ACTION_VIEW);
    intent.setData(uri);
    startActivity(intent);
  }

  /** Browser a specific index. **/
  private void browse(final File index) {
    final Intent intent = getBrowseIntent(index);
    if (intent != null) {
      startActivity(intent);
    }
  }

  /**
   * "Browse Website"
   */
  public void onBrowse(final View view) {
    browse(getTargetIndexFile());
  }

  /**
   * "Browse All Websites"
   */
  public void onBrowseAll(final View view) {
    browse(getProjectRootIndexFile());
  }

  /**
   * Fill intent with common settings (project path, etc.).
   * 
   * @param intent
   *          The intent object
   */
  protected void fillExtra(final Intent intent) {
    intent.putExtra("com.httrack.android.rootFile", getProjectRootFile());
    intent.putExtra("com.httrack.android.resourceFile", getResourceFile());
  }

  /**
   * "Cleanup"
   */
  public void onCleanup(final View view) {
    final String[] names = getProjectNames();
    if (names != null && names.length != 0) {
      final Intent intent = new Intent(this, CleanupActivity.class);
      fillExtra(intent);
      intent.putExtra("com.httrack.android.projectNames", names);
      startActivity(intent);
    }
  }

  @Override
  public void onConfigurationChanged(final Configuration newConfig) {
    // TODO: handle orientation change ?
  }

  @Override
  public boolean onOptionsItemSelected(final MenuItem item) {
    // Handle item selection
    switch (item.getItemId()) {
    case R.id.action_about:
      final String about = getString(R.string.about_credits);
      final String about0 = getString(R.string.app_name);
      final String aboutLegal = getTextResource(R.raw.legal);
      new AlertDialog.Builder(this).setTitle("About")
          .setMessage(about0 + "\n" + about + "\n\n" + aboutLegal).show();
      break;
    case R.id.action_license:
      browse(new File(new File(getResourceFile(), "license"),
          "gpl-3.0-standalone.html"));
      break;
    case R.id.action_forum:
      browse(Uri.parse("http://forum.httrack.com/"));
      break;
    case R.id.action_website:
      browse(Uri.parse("http://www.httrack.com/"));
      break;
    case R.id.action_help:
      browse(new File(new File(getResourceFile(), "html"), "index.html"));
      break;
    default:
      return super.onOptionsItemSelected(item);
    }
    return true;
  }

  /** Get current focus identifier. **/
  protected int[] getCurrentFocusId() {
    final View view = getCurrentFocus();
    if (view != null) {
      final int id = view.getId();
      if (view instanceof EditText) {
        final EditText edit = EditText.class.cast(view);
        final int start = edit.getSelectionStart();
        final int end = edit.getSelectionEnd();
        return new int[] { id, start, end };
      }
      return new int[] { id };
    } else {
      return new int[] {};
    }
  }

  /** Set current focus identifier. **/
  private void setCurrentFocusId(final int ids[]) {
    if (ids != null && ids.length != 0) {
      final int id = ids[0];
      final View view = findViewById(id);
      if (view != null) {
        view.requestFocus();
        if (view instanceof EditText) {
          final EditText edit = EditText.class.cast(view);
          if (ids.length >= 3) {
            final int start = ids[1];
            final int end = ids[2];
            edit.setSelection(start, end);
          }
        }
      }
    }
  }

  /** Save instance state. **/
  protected void saveInstanceState(final Bundle outState) {
    // Save current state. There is no guarantee than the application is going
    // to be killed/restored, however.

    // Save current pane
    savePaneFields();

    // Save settings to bundle

    // Version ID
    outState.putInt("com.httrack.android.version", versionCode);

    // Map keys
    outState.putParcelable("com.httrack.android.map", mapper.serialize());

    // Current pane
    outState.putInt("com.httrack.android.pane_id", pane_id);

    // Current focus id
    outState.putIntArray("com.httrack.android.focus_id", getCurrentFocusId());
  }

  @Override
  protected void onSaveInstanceState(final Bundle outState) {
    super.onSaveInstanceState(outState);
    saveInstanceState(outState);
  }

  /**
   * Display a notification ("toast")
   * 
   * @param message
   *          The message to be shown.
   * @param longDuration
   *          Should the notification duration be long ?
   */
  protected void showNotification(final String message,
      final boolean longDuration) {
    final Context context = getApplicationContext();
    final int duration = longDuration ? Toast.LENGTH_LONG : Toast.LENGTH_SHORT;
    final Toast toast = Toast.makeText(context, message, duration);
    toast.show();
  }

  /**
   * Display a short notification ("toast")
   * 
   * @param message
   *          The message to be shown.
   */
  protected void showNotification(final String message) {
    showNotification(message, false);
  }

  /** Send a notification. **/
  protected void sendAbortNotification() {
    final String title = getString(R.string.mirror_xxx_stopped).replace("%s",
        mapper.getProjectName());
    final String text = getString(R.string.click_on_notification_to_restart);

    // Continue an interrupted mirror
    setMap(R.id.radioAction, "0");

    // Build notification
    final Intent intent = new Intent(this, HTTrackActivity.class);
    final Bundle extras = new Bundle();
    saveInstanceState(extras);

    intent.putExtras(extras);
    sendSystemNotification(intent, title, text);
  }

  /** Send a notification with a specific Intent. **/
  protected void sendSystemNotification(final Intent intent,
      final String title, final String text) {

    // Create notification
    final long when = System.currentTimeMillis();
    final PendingIntent pintent = PendingIntent.getActivity(this, 0, intent, 0);
    final Notification notification = new NotificationCompat.Builder(this)
        .setContentTitle(title).setContentText(text).setTicker(title)
        .setSmallIcon(R.drawable.ic_launcher).setWhen(when)
        .setContentInfo(getString(R.string.start)).setContentIntent(pintent)
        .setAutoCancel(true).build();

    // Send
    final NotificationManager manager = (NotificationManager) getSystemService(Context.NOTIFICATION_SERVICE);
    manager.notify(0, notification);
  }

  /** Send a notification with a blank Intent. **/
  protected void sendSystemNotification(final String title, final String text) {
    sendSystemNotification(new Intent(), title, text);
  }

  /** Restore a saved instance state. **/
  protected void restoreInstanceState(final Bundle savedInstanceState) {
    // Check version ID
    final int version = savedInstanceState
        .getInt("com.httrack.android.version");
    if (version != versionCode) {
      Log.d(getClass().getSimpleName(), "refused bundle version " + version);
      return;
    }

    // Switch pane id
    final int id = savedInstanceState.getInt("com.httrack.android.pane_id");

    // Current focus
    final int[] focus_ids = savedInstanceState
        .getIntArray("com.httrack.android.focus_id");

    // Load map
    final Parcelable data = savedInstanceState
        .getParcelable("com.httrack.android.map");

    // Load map
    if (data != null) {
      // Load map settings
      loadParcelable(data);

      // Switch pane id (0 by default)
      setPane(id);

      // Set focus
      setCurrentFocusId(focus_ids);
    }
  }

  @Override
  protected void onRestoreInstanceState(final Bundle savedInstanceState) {
    super.onRestoreInstanceState(savedInstanceState);

    // Security
    if (runner != null) {
      showNotification(
          "restore called while running ignored! please report this warning to the developers",
          true);
      return;
    }

    restoreInstanceState(savedInstanceState);
  }

  @Override
  public void onDestroy() {
    // We are being destroyed... save profile ?
    savePaneFields();
    final String name = mapper.getProjectName();
    if (mapper.isDirty() && OptionsMapper.isStringNonEmpty(name)
        && OptionsMapper.isStringNonEmpty(mapper.getProjectUrl())) {
      try {
        serialize();
        Log.d(getClass().getSimpleName(),
            "saved profile for " + mapper.getProjectName());
      } catch (final IOException e) {
        Log.d(getClass().getSimpleName(), "could not save profile", e);
        final String title = getString(R.string.could_not_save_profile_xxx)
            .replace("%s", name);
        sendSystemNotification(title, e.getMessage());
      }
    }
    super.onDestroy();
  }
}
