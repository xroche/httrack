package com.httrack.android;

import java.io.File;
import java.io.FileInputStream;
import java.io.IOException;
import java.net.Inet6Address;
import java.net.InetAddress;
import java.net.MalformedURLException;
import java.net.NetworkInterface;
import java.net.SocketException;
import java.net.URL;
import java.util.ArrayList;
import java.util.Enumeration;
import java.util.List;

import com.httrack.android.jni.HTTrackLib;

import android.net.Uri;
import android.os.Bundle;
import android.os.Environment;
import android.os.Handler;
import android.app.Activity;
import android.app.AlertDialog;
import android.content.Intent;
import android.util.SparseArray;
import android.view.Menu;
import android.view.View;
import android.view.animation.Animation;
import android.view.animation.AnimationUtils;
import android.widget.ProgressBar;
import android.widget.TextView;

public class StartupActivity extends Activity {
	// Page layouts
	protected static final int layouts[] = { R.layout.activity_startup,
			R.layout.activity_proj_name, R.layout.activity_proj_setup,
			R.layout.activity_mirror_progress,
			R.layout.activity_mirror_finished };

	// Special layout positions
	protected static final int LAYOUT_START = 0;
	protected static final int LAYOUT_FINISHED = 4;

	// Corresponding menus
	protected static final int menus[] = { R.menu.startup, R.menu.proj_name,
			R.menu.proj_setup, R.menu.mirror_progress, R.menu.mirror_finished };

	// Fields to restore/save state (Note: might be read-only fields)
	protected static final int fields[][] = { {},
			{ R.id.fieldProjectName, R.id.fieldProjectCategory },
			{ R.id.fieldWebsiteURLs }, { R.id.fieldDisplay },
			{ R.id.fieldDisplay } };

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
	protected File target;

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
			final TextView text = (TextView) this.findViewById(R.id.textView1);
			text.append(errors);
		}
	}

	protected String getProjectName() {
		return cleanupString(map.get(R.id.fieldProjectName));
	}

	protected String getProjectUrl() {
		return cleanupString(map.get(R.id.fieldWebsiteURLs));
	}

	/**
	 * Pretty-print a string array.
	 * 
	 * @param array
	 *            The string array
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
				for (Enumeration<InetAddress> addresses = iface
						.getInetAddresses(); addresses.hasMoreElements();) {
					final InetAddress address = addresses.nextElement();
					if (address instanceof Inet6Address) {

					}
				}
			}
		} catch (SocketException se) {
		}
		return false;
	}

	/**
	 * Engine thread runner.
	 */
	protected class Runner extends Thread {
		private final HTTrackLib engine = new HTTrackLib();

		@Override
		public void run() {
			target = new File(projectPath, getProjectName());
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
			for (String s : getProjectUrl().trim().split("\\s+")) {
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
					fieldInprogress.setText("Starting mirror:\n"
							+ printArray(cargs));
				}
			});

			// Rock'in!
			String message = null;
			try {
				// Validate path
				if (!target.mkdirs() && !target.isDirectory()) {
					throw new IOException("Unable to create "
							+ target.getAbsolutePath());
				}

				// Run engine
				final int code = engine.main(cargs);
				if (code == 0) {
					message = "Success ; mirror copied in "
							+ target.getAbsolutePath();
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
						TextView.class.cast(findViewById(R.id.fieldDisplay))
								.setText("Mirror finished:\n" + displayMessage);
					}
				}
			});
		}
	}

	/**
	 * We just entered in a new pane.
	 */
	protected void onEnterNewPane() {
		switch (layouts[pane_id]) {
		case R.layout.activity_startup:
			final TextView text = (TextView) this.findViewById(R.id.textView1);
			if (version != null) {
				text.append("\n\nVERSION: " + version);
			}
			text.append("\nPATH: " + projectPath.getAbsolutePath());
			text.append("\nIPv6: " + (isIPv6Enabled() ? "YES" : "NO"));
			break;
		case R.layout.activity_mirror_progress:
			if (runner == null) {
				runner = new Runner();
				runner.start();
				TextView.class.cast(findViewById(R.id.fieldDisplay)).setText(
						"Started!");
				ProgressBar.class.cast(findViewById(R.id.progressMirror))
						.setVisibility(View.VISIBLE);
			}
			break;
		case R.layout.activity_mirror_finished:
			// TODO: ensure mirror has been terminated
			break;
		}
	}

	/**
	 * Exit button.
	 */
	protected void onFinish() {
		finish();
	}

	/**
	 * Get a specific field text.
	 * 
	 * @param id
	 *            The field ID.
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
	 *            The field ID.
	 * @param value
	 *            the associated text
	 */
	private void setFieldText(int id, String value) {
		final TextView text = (TextView) this.findViewById(id);
		text.setText(value);
	}

	/**
	 * Cleanup a space-separated string.
	 * 
	 * @param s
	 *            The string
	 * @return the cleaned up string
	 */
	private String cleanupString(String s) {
		return s.replaceAll("\\s+", " ").trim();
	}

	/**
	 * Is this space-separated string non empty ?
	 * 
	 * @param s
	 *            The string
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
			return isStringNonEmpty(getFieldText(R.id.fieldProjectName));
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
			final Animation shake = AnimationUtils.loadAnimation(this,
					R.anim.shake);
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
		if (pane_id > 0 && validatePane()) {
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
					new AlertDialog.Builder(this).setTitle("Logs")
							.setMessage(logs).show();
				} catch (IOException e) {
				}
			}
		}
	}

	/**
	 * "Browse Website"
	 */
	public void onBrowse(View view) {
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
}
