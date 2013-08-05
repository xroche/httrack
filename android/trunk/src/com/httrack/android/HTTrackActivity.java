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

import java.io.File;
import java.io.FileInputStream;
import java.io.FileOutputStream;
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
import java.util.zip.ZipEntry;
import java.util.zip.ZipInputStream;

import com.httrack.android.jni.HTTrackCallbacks;
import com.httrack.android.jni.HTTrackLib;
import com.httrack.android.jni.HTTrackStats;
import com.httrack.android.jni.HTTrackStats.Element;

import android.net.Uri;
import android.os.Bundle;
import android.os.Environment;
import android.os.Handler;
import android.app.Activity;
import android.app.AlertDialog;
import android.content.Intent;
import android.content.res.Configuration;
import android.text.Html;
import android.util.Log;
import android.view.Menu;
import android.view.View;
import android.view.animation.Animation;
import android.view.animation.AnimationUtils;
import android.widget.ArrayAdapter;
import android.widget.AutoCompleteTextView;
import android.widget.ProgressBar;
import android.widget.TextView;

public class HTTrackActivity extends Activity {
  // Page layouts
  protected static final int layouts[] = { R.layout.activity_startup,
      R.layout.activity_proj_name, R.layout.activity_proj_setup,
      R.layout.activity_mirror_progress, R.layout.activity_mirror_finished };

  // Special layout positions
  protected static final int LAYOUT_START = 0;
  protected static final int LAYOUT_FINISHED = 4;

  // Corresponding menus
  protected static final int menus[] = { R.menu.startup, R.menu.proj_name,
      R.menu.proj_setup, R.menu.mirror_progress, R.menu.mirror_finished };

  // Fields to restore/save state (Note: might be read-only fields)
  protected static final int fields[][] = { {},
      { R.id.fieldProjectName, R.id.fieldProjectCategory },
      { R.id.fieldWebsiteURLs }, { R.id.fieldDisplay }, { R.id.fieldDisplay } };

  // Options mapper, containing all options
  protected final OptionsMapper mapper = new OptionsMapper();

  // Activity identifier when using startActivityForResult()
  protected static final int ACTIVITY_OPTIONS = 0;

  // Engine
  protected Runner runner = null;

  // Current pane id and context
  protected int pane_id = -1;

  // Handler to execute code in UI thread
  private Handler handlerUI = new Handler();

  // Project settings
  protected String version;
  protected File rootPath;
  protected File httrackPath;
  protected File projectPath;
  protected boolean mirrorRefresh;

  // Stats
  protected HTTrackStats lastStats;

  private static File getExternalStorage() throws IOException {
    final String state = Environment.getExternalStorageState();

    if (Environment.MEDIA_MOUNTED.equals(state)) {
      return Environment.getExternalStorageDirectory();
    } else if (Environment.MEDIA_MOUNTED_READ_ONLY.equals(state)) {
      throw new IOException("read-only media");
    } else {
      throw new IOException("no storage media");
    }
  }

  @Override
  protected void onCreate(Bundle savedInstanceState) {
    super.onCreate(savedInstanceState);

    // Attempt to load the native library.
    String errors = "";
    try {
      // Initialize
      HTTrackLib.init();

      // Fetch httrack engine version
      version = HTTrackLib.getVersion();

      // Extract resources if necessary
      getResourceFile();
    } catch (final RuntimeException re) {
      // Woops, something is not right here.
      errors += "\n\nERROR: " + re.getMessage();
    }

    // Default target directory on external storage
    try {
      rootPath = getExternalStorage();
    } catch (final IOException e) {
      errors += "\n\nWARNING: " + e.getMessage();
      projectPath = Environment.getExternalStorageDirectory();
    }
    httrackPath = new File(rootPath, "HTTrack");
    projectPath = new File(httrackPath, "Websites");

    // Ensure users can see us
    HTTrackActivity.setFileReadWrite(httrackPath);
    HTTrackActivity.setFileReadWrite(projectPath);

    // Go to first pane now
    setPane(0);

    // First pane text error
    if (errors != null) {
      final TextView text = (TextView) this.findViewById(R.id.fieldDisplay);
      text.append(errors);
    }
  }

  /** Get the resource directory. Create it if necessary. **/
  private File getResourceFile() {
    final File rscPath = new File(httrackPath, "resources");
    if (!rscPath.exists()) {
      if (HTTrackActivity.mkdirs(rscPath)) {
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
              byte[] bytes = new byte[1024];
              int length;
              while ((length = file.read(bytes)) >= 0) {
                writer.write(bytes, 0, length);
              }
              writer.close();
              dest.setLastModified(entry.getTime());
            }
          }
          file.close();
          zipStream.close();
        } catch (final IOException io) {
          rscPath.delete();
        }
      }
    }
    return rscPath;
  }

  /**
   * Get a map value.
   * 
   * @param key
   *          The key
   * @return The value
   */
  public String getMap(int key) {
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
  public void setMap(int key, final String value) {
    mapper.setMap(key, value);
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
      return new File(projectPath, name);
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
   * Pretty-print a string array.
   * 
   * @param array
   *          The string array
   * @return The pretty-printed value
   */
  private String printArray(final String[] array) {
    final StringBuilder builder = new StringBuilder();
    for (String s : array) {
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
      final File dumpFile = new File(new File(Environment
          .getExternalStorageDirectory().getPath(), "HTTrack"), "error.txt");
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
  protected int buildTopIndex() {
    // Build top index
    final File rsc = getResourceFile();
    if (rsc != null) {
      return HTTrackLib.buildTopIndex(getProjectRootFile(), rsc);
    } else {
      return 0;
    }
  }

  /**
   * Engine thread runner.
   */
  protected class Runner extends Thread implements HTTrackCallbacks {
    private final HTTrackLib engine = new HTTrackLib(this);
    private boolean stopped;
    final StringBuilder str = new StringBuilder();

    @Override
    public void run() {
      try {
        runInternal();
      } catch (final Throwable e) {
        HTTrackActivity.emergencyDump(e);
      }
    }

    protected void runInternal() {
      final File target = getTargetFile();
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
      for (final String cmd : mapper.buildCommandline()) {
        args.add(cmd);
      }

      // Final args array
      final String[] cargs = args.toArray(new String[] {});
      Log.v(this.getClass().getName(), "starting engine: " + printArray(cargs));

      // Fancy message
      handlerUI.post(new Runnable() {
        @Override
        public void run() {
          final TextView fieldInprogress = (TextView) findViewById(R.id.fieldDisplay);
          fieldInprogress.setText("Starting mirror:\n" + printArray(cargs));
        }
      });

      // Rock'in!
      String message = null;
      try {
        // Validate path
        if (!HTTrackActivity.mkdirs(target)) {
          throw new IOException("Unable to create " + target.getAbsolutePath());
        }
        HTTrackActivity.setFileReadWrite(target);

        // Serialize settings
        serialize();

        // Run engine
        final int code = engine.main(cargs);

        // Result
        if (code == 0) {
          if (lastStats.errorsCount == 0) {
            message = "<b>Success</b>!";
          } else if (lastStats.filesWritten != 0) {
            message = "<b>Success</b>! (" + lastStats.errorsCount + " errors)";
          } else {
            message = "<b>Failed</b>! (" + lastStats.errorsCount
                + " errors, no files written)";
          }
          message += "<br /><br />Mirror copied in <i>"
              + target.getAbsolutePath() + "</i>:";
          message += "<br /><i>";
          for (final String f : target.list()) {
            message += f;
            message += "<br />";
          }
          message += "</i>";

        } else {
          message = "<b>Error</b> (<i>code " + code + "</i>)";
        }

        // Build top index
        buildTopIndex();
      } catch (final IOException io) {
        message = io.getMessage();
      }

      // Ensure we switch to the final pane
      final String displayMessage = "Mirror finished: " + message;
      final long errorsCount = lastStats != null ? lastStats.errorsCount : 0;
      handlerUI.post(new Runnable() {
        @Override
        public void run() {
          // Final pane
          setPane(LAYOUT_FINISHED);

          // Fancy result message
          if (displayMessage != null) {
            TextView.class.cast(findViewById(R.id.fieldDisplay)).setText(
                Html.fromHtml(displayMessage));
          }
          if (errorsCount != 0) {
            final Animation shake = AnimationUtils.loadAnimation(
                HTTrackActivity.this, R.anim.scale);
            findViewById(R.id.buttonLogs).startAnimation(shake);
          }
        }
      });
    }

    /**
     * Stop the mirror.
     */
    public void stopMirror() {
      synchronized (this) {
        stopped = true;
      }
      engine.stop(true);
    }

    /**
     * Has the mirror stopped ?
     */
    public synchronized boolean isStopped() {
      return stopped;
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
      if (isStopped()) {
        return;
      }

      // build stats infos
      final String sep = " • ";
      str.setLength(0);
      str.append("<b>Bytes saved</b>: ");
      str.append(stats.bytesWritten);
      str.append(sep);
      str.append("<b>Links scanned</b>: ");
      str.append(stats.linksScanned);
      str.append("/");
      str.append(stats.linksTotal);
      str.append(" (+");
      str.append(stats.linksBackground);
      str.append(")<br />");
      /* */
      str.append("<b>Time</b>: ");
      str.append(stats.elapsedTime);
      str.append(sep);
      str.append("<b>Files written</b>: ");
      str.append(stats.filesWritten);
      str.append(" (+");
      str.append(stats.filesWrittenBackground);
      str.append(")<br />");
      /* */
      str.append("<b>Transfer rate</b>: ");
      str.append(stats.transferRate);
      str.append(" (");
      str.append(stats.totalTransferRate);
      str.append(")");
      str.append(sep);
      str.append("<b>Files updated</b>: ");
      str.append(stats.filesUpdated);
      str.append("<br />");
      /* */
      str.append("<b>Active connections</b>: ");
      str.append(stats.socketsCount);
      str.append(sep);
      str.append("<b>Errors</b>:");
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

          // url
          final int max_len = 32;
          final String s = element.address + element.filename;
          // cut string if necessary
          if (s.length() > max_len + 1) {
            str.append(s.substring(0, max_len / 2));
            str.append("…");
            str.append(s.substring(s.length() - max_len / 2));
          } else {
            str.append(s);
          }
          str.append(" → ");

          // state
          switch (element.state) {
          case Element.STATE_CONNECTING:
            str.append("connecting");
            break;
          case Element.STATE_DNS:
            str.append("dns");
            break;
          case Element.STATE_FTP:
            str.append("ftp");
            break;
          case Element.STATE_READY:
            str.append("ready");
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
      handlerUI.post(new Runnable() {
        @Override
        public void run() {
          TextView.class.cast(findViewById(R.id.fieldDisplay)).setText(
              Html.fromHtml(message));
        }
      });
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
   * Is there an existing profile yet ?
   * 
   * @return true if there is a winprofile.ini file
   */
  protected boolean hasProfileFile() {
    final File profile = getProfileFile();
    return profile != null && profile.exists();
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
  protected void serialize() throws IOException {
    final File target = getTargetFile();
    final File profile = getProfileFile();
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
   * We just entered in a new pane.
   */
  protected void onEnterNewPane() {
    switch (layouts[pane_id]) {
    case R.layout.activity_startup:
      final TextView text = TextView.class.cast(this
          .findViewById(R.id.fieldDisplay));

      // Welcome message.
      final String html = getString(R.string.welcome_message);
      final StringBuilder str = new StringBuilder(html);

      // Debugging and information.
      str.append("<br /><i>");
      if (version != null) {
        str.append("<br />VERSION: ");
        str.append(version);
      }
      str.append(" • PATH: ");
      str.append(projectPath.getAbsolutePath());
      str.append(" • IPv6: ");
      final InetAddress addrV6 = getIPv6Address();
      str.append(addrV6 != null ? ("YES (" + addrV6.getHostAddress() + ")")
          : "NO");
      str.append("</i>");
      text.setText(Html.fromHtml(str.toString()));

      // Enable or disable browse & cleanup button depending on existing
      // project(s)
      View.class.cast(this.findViewById(R.id.buttonClear)).setEnabled(
          hasProjectNames());
      View.class.cast(this.findViewById(R.id.buttonBrowseAll)).setEnabled(
          hasProjectRootIndexFile());

      break;
    case R.layout.activity_proj_name:
      final String[] names = getProjectNames();
      if (names != null) {
        final AutoCompleteTextView name = AutoCompleteTextView.class.cast(this
            .findViewById(R.id.fieldProjectName));
        Log.v(this.getClass().getName(), "project names: " + printArray(names));
        name.setAdapter(new ArrayAdapter<String>(this,
            android.R.layout.simple_dropdown_item_1line, names));
      }
      break;
    case R.layout.activity_proj_setup:
      final boolean hasProfile = hasProfileFile();
      View.class.cast(this.findViewById(R.id.radioAction)).setEnabled(
          hasProfile);
      View.class.cast(this.findViewById(R.id.radioAction)).setVisibility(
          hasProfile ? View.VISIBLE : View.INVISIBLE);
      break;
    case R.layout.activity_mirror_progress:
      if (runner == null) {
        runner = new Runner();
        runner.start();
        TextView.class.cast(findViewById(R.id.fieldDisplay))
            .setText("Started!");
        ProgressBar.class.cast(findViewById(R.id.progressMirror))
            .setVisibility(View.VISIBLE);
      }
      break;
    case R.layout.activity_mirror_finished:
      // Ensure the engine has stopped running
      if (runner != null) {
        runner.stopMirror();
      }

      // Enable browse button if index.html exists
      View.class.cast(this.findViewById(R.id.buttonBrowse)).setEnabled(
          hasTargetIndexFile());

      // Final stats
      if (lastStats != null && lastStats.errorsCount != 0) {
        // TODO
      }
      break;
    }
  }

  /**
   * Exit button.
   */
  protected void onFinish() {
    // FIXME TODO CRASHES!
    runOnUiThread(new Runnable() {
      public void run() {
        finish();
      }
    });
  }

  /**
   * Get a specific field text.
   * 
   * @param id
   *          The field ID.
   * @return the associated text
   */
  private String getFieldText(int id) {
    final TextView text = (TextView) this.findViewById(id);
    return text.getText().toString();
  }

  /**
   * Set a specific field text value.
   * 
   * @param id
   *          The field ID.
   * @param value
   *          the associated text
   */
  private void setFieldText(int id, String value) {
    final TextView text = (TextView) this.findViewById(id);
    text.setText(value);
  }

  /**
   * Is this space-separated string non empty ?
   * 
   * @param s
   *          The string
   * @return true if the string was not empty
   */
  private boolean isStringNonEmpty(String s) {
    return OptionsMapper.cleanupString(s).length() != 0;
  }

  /**
   * Validate the current pane
   * 
   * @return true if the current pane is valid
   */
  protected boolean validatePane() {
    switch (pane_id) {
    case 1:
      final String name = getFieldText(R.id.fieldProjectName);
      if (isStringNonEmpty(name)) {
        // We need to put immediately the name in the map to be able to
        // unserialize.
        try {
          setMap(R.id.fieldProjectName, name);
          unserialize();
        } catch (final IOException e) {
          // Ignore (if not found)
        }
        return true;
      }
      return false;
    case 2:
      return isStringNonEmpty(getFieldText(R.id.fieldWebsiteURLs));
    }
    return true;
  }

  /**
   * Validate the current pane with visual effects on error. Thanks to Sushant
   * for the idea.
   * 
   * @return true if the current pane is valid
   */
  protected boolean validatePaneWithEffects(boolean next) {
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

  private void setPane(int position) {
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
  public boolean onCreateOptionsMenu(Menu menu) {
    // Inflate the menu; this adds items to the action bar if it is present.
    if (pane_id != -1) {
      getMenuInflater().inflate(menus[pane_id], menu);
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
   * "Options"
   */
  public void onClickOptions(final View view) {
    // First save current pane settings
    savePaneFields();

    // Then start new activity
    final Intent intent = new Intent(this, OptionsActivity.class);
    fillExtra(intent);
    intent.putExtra("map", mapper.getMap());
    Log.d(this.getClass().getName(), "map size: " + mapper.getMap().size());
    startActivityForResult(intent, ACTIVITY_OPTIONS);
  }

  @Override
  protected void onActivityResult(int requestCode, int resultCode,
      final Intent data) {
    super.onActivityResult(requestCode, resultCode, data);
    switch (requestCode) {
    case ACTIVITY_OPTIONS:
      if (resultCode == Activity.RESULT_OK) {
        // Load modified map
        mapper.getMap().unserialize(data.getParcelableExtra("map"));
        Log.d(this.getClass().getName(), "received map size: "
            + mapper.getMap().size());

        // Load possibly modified field(s)
        loadPaneFields();
      }
      break;
    }
  }

  /**
   * "Show Logs"
   */
  public void onShowLogs(final View view) {
    final File target = getTargetFile();

    if (target != null && target.exists()) {
      final File log = new File(target, "hts-log.txt");
      if (log.exists()) {
        FileInputStream rd;
        try {
          rd = new FileInputStream(log);
          byte[] data = new byte[(int) log.length()];
          rd.read(data);
          rd.close();
          final String logs = new String(data, "UTF-8");
          new AlertDialog.Builder(this).setTitle("Logs").setMessage(logs)
              .show();
        } catch (final IOException e) {
        }
      }
    }
  }

  /** Browser a specific index. **/
  private void browse(final File index) {
    if (index != null && index.exists()) {
      final Intent intent = new Intent();
      intent.setAction(android.content.Intent.ACTION_VIEW);
      // Without the MIME, Android tend to crash with a NPE (!)
      intent.setDataAndType(Uri.fromFile(index), "text/html");
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
    intent.putExtra("rootFile", getProjectRootFile());
    intent.putExtra("resourceFile", getResourceFile());
  }

  /**
   * "Browse Website"
   */
  public void onCleanup(final View view) {
    final String[] names = getProjectNames();
    if (names != null && names.length != 0) {
      final Intent intent = new Intent(this, CleanupActivity.class);
      fillExtra(intent);
      intent.putExtra("projectNames", names);
      startActivity(intent);
    }
  }

  @Override
  public void onConfigurationChanged(Configuration newConfig) {
    // TODO: handle orientation change ?
  }
}
