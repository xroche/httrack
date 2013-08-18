package com.httrack.android;

import java.io.File;
import java.util.ArrayList;
import java.util.List;

import android.os.Bundle;
import android.os.Handler;
import android.app.Activity;
import android.content.Intent;
import android.util.Pair;
import android.view.Gravity;
import android.view.View;
import android.view.ViewGroup.LayoutParams;
import android.widget.Button;
import android.widget.EditText;
import android.widget.LinearLayout;
import android.widget.TextView;

public class FileChooserActivity extends Activity implements
    View.OnClickListener {
  // Borderless buttons ?
  protected static final boolean borderless = android.os.Build.VERSION.SDK_INT >= 11;

  protected File projectRootFile;
  protected final List<Pair<String, File>> files = new ArrayList<Pair<String, File>>();

  // Handler to execute code in UI thread
  private final Handler handlerUI = new Handler();

  /*
   * Convert dp (Density-independent Pixels) to px (pixel)
   */
  private int dpToPx(final double dp) {
    final float scale = getResources().getDisplayMetrics().density;
    int px = (int) (dp * scale + 0.5f);
    return px;
  }

  /*
   * Create all tabs for main options menu.
   */
  private void refreshDir() {
    // Set text
    final EditText base = EditText.class.cast(findViewById(R.id.fieldBasePath));
    final String path = projectRootFile.getAbsolutePath();
    base.setText(projectRootFile.getAbsolutePath());
    base.setSelection(path.length());

    // List directory
    final File[] list = projectRootFile != null ? projectRootFile.listFiles()
        : null;
    if (list != null) {
      files.clear();
      final File parent = projectRootFile.getParentFile();
      if (parent != null) {
        files.add(new Pair<String, File>(getString(R.string.ellipsis), parent));
      }
      for (final File file : list) {
        if (file.isDirectory() && !file.isHidden() /* && file.canWrite() */) {
          files.add(new Pair<String, File>(file.getName(), file));
        }
      }
    }

    if (files != null) {
      // Create tabs
      final LinearLayout scroll = LinearLayout.class
          .cast(findViewById(R.id.layout));
      scroll.removeAllViews();

      // Add all buttons linking to options
      for (int i = 0; i < files.size(); i++) {
        // Add separator
        if (borderless && i != 0) {
          final View line = new View(this, null, R.style.DividerLineHorizontal);

          // FIXME TODO: why in hell isn't my shiny style NOT working ?
          final LinearLayout.MarginLayoutParams layout = new LinearLayout.MarginLayoutParams(
              LayoutParams.FILL_PARENT, dpToPx(1));
          layout.bottomMargin = dpToPx(8);
          layout.topMargin = dpToPx(8);
          line.setLayoutParams(layout);
          line.setBackgroundColor(getResources().getColor(R.color.black));

          // Add line
          scroll.addView(line);
        }

        // Next tab class
        final Pair<String, File> entry = files.get(i);
        final String title = entry.first;

        // Create a flat button ; see
        // <http://developer.android.com/guide/topics/ui/controls/button.html#Borderless>
        // Added in API level 11
        final Button button = borderless ? new Button(this, null,
            android.R.attr.borderlessButtonStyle) : new Button(this);
        if (borderless) {
          // Left-aligned text
          button.setGravity(Gravity.LEFT | Gravity.CENTER_VERTICAL);
        }

        // Title
        button.setText(title);

        // Set listener
        button.setOnClickListener(this);

        // Set tag to our index
        button.setTag(i);

        // And finally add button
        scroll.addView(button);
      }
    }
  }

  @Override
  protected void onCreate(final Bundle savedInstanceState) {
    super.onCreate(savedInstanceState);
    setContentView(R.layout.activity_file_chooser);

    // Fetch args from parent
    final Bundle extras = getIntent().getExtras();
    projectRootFile = File.class.cast(extras
        .get("com.httrack.android.rootFile"));
    if (projectRootFile == null) {
      throw new RuntimeException("internal error");
    }

    // Initial dir
    refreshDir();
  }

  @Override
  public void onClick(final View v) {
    // Which button was clicked ?
    final Button cb = Button.class.cast(v);

    // Fetch tag position
    final int position = Integer.parseInt(cb.getTag().toString());

    if (position >= 0 && files != null && position < files.size()) {
      projectRootFile = files.get(position).second;
    }
    handlerUI.post(new Runnable() {
      @Override
      public void run() {
        refreshDir();
      }
    });
  }

  public void onClickApply(final View v) {
    // Selection
    final TextView base = TextView.class.cast(findViewById(R.id.fieldBasePath));
    final String path = base.getText().toString().trim();

    // Declare result
    final Intent intent = new Intent();
    intent.putExtra("com.httrack.android.rootFile", path);
    setResult(Activity.RESULT_OK, intent);

    // Finish activity
    finish();
  }
}
