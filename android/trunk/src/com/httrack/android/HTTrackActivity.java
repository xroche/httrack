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
import java.io.FileReader;
import java.io.FileWriter;
import java.io.IOException;
import java.io.PrintWriter;
import java.net.Inet6Address;
import java.net.InetAddress;
import java.net.MalformedURLException;
import java.net.NetworkInterface;
import java.net.SocketException;
import java.net.URL;
import java.net.URLDecoder;
import java.net.URLEncoder;
import java.util.ArrayList;
import java.util.Enumeration;
import java.util.List;

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
import android.text.Html;
import android.util.Pair;
import android.util.SparseArray;
import android.view.Menu;
import android.view.View;
import android.view.animation.Animation;
import android.view.animation.AnimationUtils;
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

  // Fields used in serialization
  @SuppressWarnings("unchecked")
  protected static final Pair<Integer, String> fieldsSerializer[] = new Pair[] {
      new Pair<Integer, String>(R.id.fieldProjectName, "projectName"),
      new Pair<Integer, String>(R.id.fieldProjectCategory, "projectCategory"),
      new Pair<Integer, String>(R.id.fieldWebsiteURLs, "websiteURLs") };

  // Engine
  protected Runner runner = null;

  // Current pane id and context
  protected int pane_id = -1;
  protected final SparseArray<String> map = new SparseArray<String>();

  // Handler to execute code in UI thread
  private Handler handlerUI = new Handler();

  // Project settings
  protected String version;
  protected File rootPath;
  protected File projectPath;

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
    } catch (RuntimeException re) {
      // Woops, something is not right here.
      errors += "\n\nERROR: " + re.getMessage();
    }

    // Default target directory on external storage
    try {
      rootPath = getExternalStorage();
    } catch (IOException e) {
      errors += "\n\nWARNING: " + e.getMessage();
      projectPath = Environment.getExternalStorageDirectory();
    }
    projectPath = new File(new File(rootPath, "HTTrack"), "Websites");

    // Go to first pane now
    setPane(0);

    // First pane text error
    if (errors != null) {
      final TextView text = (TextView) this.findViewById(R.id.fieldDisplay);
      text.append(errors);
    }
  }

  protected String getProjectName() {
    return cleanupString(map.get(R.id.fieldProjectName));
  }

  /**
   * Get the destination directory for the current project.
   * 
   * @return The destination directory.
   */
  protected File getTargetFile() {
    final String name = getProjectName();
    if (name != null && name.length() != 0) {
      return new File(projectPath, name);
    } else {
      return null;
    }
  }

  protected String getProjectUrl() {
    return cleanupString(map.get(R.id.fieldWebsiteURLs));
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
   * is IPv6 available on this phone ?
   * 
   * @return true if IPv6 is available on this phone
   */
  private static boolean isIPv6Enabled() {
    try {
      for (final Enumeration<NetworkInterface> interfaces = NetworkInterface
          .getNetworkInterfaces(); interfaces.hasMoreElements();) {
        final NetworkInterface iface = interfaces.nextElement();
        for (final Enumeration<InetAddress> addresses = iface
            .getInetAddresses(); addresses.hasMoreElements();) {
          final InetAddress address = addresses.nextElement();
          if (address instanceof Inet6Address) {
            return true;
          }
        }
      }
    } catch (SocketException se) {
    }
    return false;
  }

  /**
   * Emergency dump.
   */
  protected static void emergencyDump(final Throwable e) {
    try {
      final FileWriter writer = new FileWriter(Environment
          .getExternalStorageDirectory().getPath() + "/HTTrack/error.txt");
      final PrintWriter print = new PrintWriter(writer);
      e.printStackTrace(print);
      writer.close();
    } catch (IOException io) {
    }
  }

  /**
   * Engine thread runner.
   */
  protected class Runner extends Thread implements HTTrackCallbacks {
    private final HTTrackLib engine = new HTTrackLib(this);

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

      // TEMPORARY FIXME
      args.add("--max-time");
      args.add("60");
      args.add("--max-size");
      args.add("10000000");
      // TEMPORARY FIXME

      // Target
      args.add("-O");
      args.add(target.getAbsolutePath());

      // Add URLs
      for (final String s : getProjectUrl().trim().split("\\s+")) {
        if (s.length() != 0) {
          args.add(s);
        }
      }

      // Final args
      final String[] cargs = args.toArray(new String[] {});

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
        if (!target.mkdirs() && !target.isDirectory()) {
          throw new IOException("Unable to create " + target.getAbsolutePath());
        }

        // Serialize settings
        serialize();

        // Run engine
        final int code = engine.main(cargs);
        if (code == 0) {
          message = "Success ; mirror copied in " + target.getAbsolutePath();
          message += "\n";
          for (final String f : target.list()) {
            message += "\n\t" + f;
          }

        } else {
          message = "Error code " + code;
        }
      } catch (IOException io) {
        message = io.getMessage();
      }

      // Ensure we switch to the final pane
      final String displayMessage = message;
      handlerUI.post(new Runnable() {
        @Override
        public void run() {
          // Final pane
          setPane(LAYOUT_FINISHED);

          // Fancy result message
          if (displayMessage != null) {
            TextView.class.cast(findViewById(R.id.fieldDisplay)).setText(
                "Mirror finished:\n" + displayMessage);
          }
        }
      });
    }

    /**
     * Stop the mirror.
     */
    public void stopMirror() {
      engine.stop(true);
    }

    @Override
    public void onRefresh(HTTrackStats stats) {
      if (stats == null) {
        return;
      }
      final String sep = " • ";
      final StringBuilder str = new StringBuilder();
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
  private File getProfileFile() {
    final File target = getTargetFile();
    if (target != null) {
      return new File(target, "andprofile.ini");
    } else {
      return null;
    }
  }

  /**
   * Serialize current profile to disk.
   * 
   * @throws IOException
   *           Upon I/O error.
   */
  protected void serialize() throws IOException {
    final File target = getTargetFile();

    // Validate path
    if (!target.mkdirs() && !target.isDirectory()) {
      throw new IOException("Unable to create " + target.getAbsolutePath());
    }

    // Write settings
    final File profile = getProfileFile();
    final FileWriter writer = new FileWriter(profile);
    final BufferedWriter lwriter = new BufferedWriter(writer);
    try {
      for (final Pair<Integer, String> field : fieldsSerializer) {
        final String value = map.get(field.first);
        final String key = field.second;
        lwriter.write(key);
        lwriter.write("=");
        lwriter.write(URLEncoder.encode(value, "UTF-8"));
        lwriter.write("\n");
      }
      lwriter.close();
    } finally {
      writer.close();
    }
  }

  /**
   * Unserialize profile from disk.
   * 
   * @throws IOException
   *           Upon I/O error.
   */
  protected void unserialize() throws IOException {
    // Write settings
    final File profile = getProfileFile();
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
          for (final Pair<Integer, String> field : fieldsSerializer) {
            final int id = field.first;
            final String fkey = field.second;
            if (fkey.equals(key)) {
              map.put(id, value);
              break;
            }
          }
        }
      }
      lreader.close();
    } finally {
      reader.close();
    }
  }

  /**
   * We just entered in a new pane.
   */
  protected void onEnterNewPane() {
    switch (layouts[pane_id]) {
    case R.layout.activity_startup:
      final TextView text = (TextView) this.findViewById(R.id.fieldDisplay);

      // Welcome message.
      final String html = getString(R.string.welcome_message);
      final StringBuilder str = new StringBuilder(html);

      // Debugging.
      str.append("<br /><i>");
      if (version != null) {
        str.append("<br />VERSION: ");
        str.append(version);
      }
      str.append("<br />PATH: ");
      str.append(projectPath.getAbsolutePath());
      str.append("<br />IPv6: ");
      str.append(isIPv6Enabled() ? "YES" : "NO");
      str.append("</i>");

      // FIXME TODO: why do we need to to this by ourselves ?
      text.setText(Html.fromHtml(str.toString()));
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
      if (runner != null) {
        runner.stopMirror();
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
   * Cleanup a space-separated string.
   * 
   * @param s
   *          The string
   * @return the cleaned up string
   */
  private String cleanupString(String s) {
    return s.replaceAll("\\s+", " ").trim();
  }

  /**
   * Is this space-separated string non empty ?
   * 
   * @param s
   *          The string
   * @return true if the string was not empty
   */
  private boolean isStringNonEmpty(String s) {
    return cleanupString(s).length() != 0;
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
          map.put(R.id.fieldProjectName, name);
          unserialize();
        } catch (IOException e) {
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

  private void setPane(int position) {
    if (pane_id != position) {
      // Leaving a pane: save data
      if (pane_id != -1) {
        for (final int id : fields[pane_id]) {
          final String value = getFieldText(id);
          map.put(id, value);
        }
      }

      // Switch pane
      pane_id = position;
      setContentView(layouts[pane_id]);

      // Entering a new pane: restore data
      for (final int id : fields[pane_id]) {
        final String value = map.get(id);
        setFieldText(id, value);
      }

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
  public void onClickNext(View view) {
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
  public void onClickPrevious(View view) {
    if (pane_id > 0) {
      setPane(pane_id - 1);
    }
  }

  /**
   * "Options"
   */
  public void onClickOptions(View view) {
  }

  /**
   * "Show Logs"
   */
  public void onShowLogs(View view) {
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
        } catch (IOException e) {
        }
      }
    }
  }

  /**
   * "Browse Website"
   */
  public void onBrowse(View view) {
    final File target = getTargetFile();

    // FIXME TODO CRASHES!
    runOnUiThread(new Runnable() {
      public void run() {
        if (target != null && target.exists()) {
          final File index = new File(target, "index.html");
          if (index.exists()) {
            try {
              final URL url = index.toURI().toURL();
              final Intent browser = new Intent(Intent.ACTION_VIEW,
                  Uri.parse(url.toString()));
              startActivity(browser);
            } catch (MalformedURLException e) {
            }
          }
        }
      }
    });
  }
}
