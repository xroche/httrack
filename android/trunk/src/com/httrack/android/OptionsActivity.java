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

import java.lang.annotation.ElementType;
import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;
import java.lang.annotation.Target;

import android.app.Activity;
import android.content.Intent;
import android.os.Bundle;
import android.util.DisplayMetrics;
import android.util.Log;
import android.view.Gravity;
import android.view.View;
import android.view.ViewGroup;
import android.view.ViewGroup.LayoutParams;
import android.widget.Button;
import android.widget.LinearLayout;

/**
 * The options activity.
 */
public class OptionsActivity extends Activity implements View.OnClickListener {
  /* List of all tabs. */
  @SuppressWarnings("unchecked")
  protected static Class<? extends Tab>[] tabClasses = new Class[] {
      ScanRulesTab.class, LimitsTab.class, FlowControlTab.class,
      LinksTab.class, BuildTab.class, BrowserId.class, Spider.class,
      Proxy.class, LogIndexCache.class, MimeDefs.class, ExpertsOnly.class };

  // The parent map
  protected final SparseArraySerializable map = new SparseArraySerializable();

  // Widget data exchanger
  private final WidgetDataExchange widgetDataExchange = new WidgetDataExchange(
      this);

  // Current activity class
  protected Class<?> activityClass;

  // use large screen ? (tablets)
  protected boolean isTabletMode;

  /**
   * The tab activit(ies) common interface.
   */
  public abstract static interface Tab {
  }

  /**
   * Title of a tab class.
   */
  @Retention(RetentionPolicy.RUNTIME)
  @Target(ElementType.TYPE)
  public static @interface Title {
    int value();
  }

  /**
   * Activity ID of a tab class.
   */
  @Retention(RetentionPolicy.RUNTIME)
  @Target(ElementType.TYPE)
  public static @interface ActivityId {
    int value();
  }

  /**
   * List of fields of a tab class.
   */
  @Retention(RetentionPolicy.RUNTIME)
  @Target(ElementType.TYPE)
  public static @interface Fields {
    int[] value();
  }

  @Title(R.string.scan_rules)
  @ActivityId(R.layout.activity_options_scanrules)
  @Fields({ R.id.editRules })
  public static class ScanRulesTab implements Tab {
  }

  @Title(R.string.limits)
  @ActivityId(R.layout.activity_options_limits)
  @Fields({ R.id.editMaxDepth, R.id.editMaxExtDepth, R.id.editMaxSizeHtml,
      R.id.editMaxSizeOther, R.id.editSiteSizeLimit, R.id.editMaxTimeOverall,
      R.id.editMaxTransferRate, R.id.editMaxConnectionsSecond,
      R.id.editMaxNumberLinks })
  public static class LimitsTab implements Tab {
  }

  @Title(R.string.flow_control)
  @ActivityId(R.layout.activity_options_flowcontrol)
  @Fields({ R.id.editNumberOfConnections, R.id.checkPersistentConnections,
      R.id.editTimeout, R.id.checkRemoveHostIfTimeout, R.id.editRetries,
      R.id.editMinTransferRate, R.id.checkRemoveHostIfSlow })
  public static class FlowControlTab implements Tab {
  }

  @Title(R.string.links)
  @ActivityId(R.layout.activity_options_links)
  @Fields({ R.id.checkDetectAllLinks, R.id.checkGetNonHtmlNear,
      R.id.checkTestAllLinks, R.id.checkGetHtmlFirst })
  public static class LinksTab implements Tab {
  }

  @Title(R.string.build)
  @ActivityId(R.layout.activity_options_build)
  @Fields({ R.id.checkDosNames, R.id.checkIso9660, R.id.checkNoErrorPages,
      R.id.checkNoExternalPages, R.id.checkHidePasswords,
      R.id.checkHideQueryStrings, R.id.checkDoNotPurge, R.id.radioBuild,
      R.id.editCustomBuild })
  public static class BuildTab implements Tab {
  }

  @Title(R.string.browser_id)
  @ActivityId(R.layout.activity_options_browserid)
  @Fields({ R.id.editBrowserIdentity, R.id.editHtmlFooter,
      R.id.editAcceptLanguage, R.id.editOtherHeaders, R.id.editDefaultReferer })
  public static class BrowserId implements Tab {
  }

  @Title(R.string.spider)
  @ActivityId(R.layout.activity_options_spider)
  @Fields({ R.id.checkAcceptCookies, R.id.radioCheckDocumentType,
      R.id.checkParseJavaFiles, R.id.radioSpider, R.id.checkUpdateHacks,
      R.id.checkUrlHacks, R.id.checkTolerentRequests, R.id.checkForceHttp10 })
  public static class Spider implements Tab {
  }

  @Title(R.string.proxy)
  @ActivityId(R.layout.activity_options_proxy)
  @Fields({ R.id.editProxy, R.id.editProxyPort, R.id.checkUseProxyForFtp })
  public static class Proxy implements Tab {
  }

  @Title(R.string.log_index_cache)
  @ActivityId(R.layout.activity_options_logindexcache)
  @Fields({ R.id.checkStoreAllFilesInCache,
      R.id.checkDoNotRedownloadLocallErasedFiles, R.id.checkCreateLogFiles,
      R.id.radioVerbosity, R.id.checkUseIndex, R.id.checkUseWordIndex,
      R.id.checkUseMailIndex })
  public static class LogIndexCache implements Tab {
  }

  @Title(R.string.type_mime_associations)
  @ActivityId(R.layout.activity_options_mimetypes)
  @Fields({ R.id.editExtDef1, R.id.editMimeDef1, R.id.editExtDef2,
      R.id.editMimeDef2, R.id.editExtDef3, R.id.editMimeDef3, R.id.editExtDef4,
      R.id.editMimeDef4, R.id.editExtDef5, R.id.editMimeDef5, R.id.editExtDef6,
      R.id.editMimeDef6, R.id.editExtDef7, R.id.editMimeDef7, R.id.editExtDef8,
      R.id.editMimeDef8 })
  public static class MimeDefs implements Tab {
  }

  @Title(R.string.experts_only)
  @ActivityId(R.layout.activity_options_expertsonly)
  @Fields({ R.id.checkUseCacheForUpdates, R.id.radioPrimaryScanRule,
      R.id.textTravelMode, R.id.radioTravelMode, R.id.radioGlobalTravelMode,
      R.id.radioRewriteLinks, R.id.checkActivateDebugging })
  public static class ExpertsOnly implements Tab {
  }

  /*
   * Convert dp (Density-independent Pixels) to px (pixel)
   */
  private int dpToPx(final double dp) {
    final float scale = getResources().getDisplayMetrics().density;
    int px = (int) (dp * scale + 0.5f);
    return px;
  }

  /*
   * Is the screen larger than <width>x<height> ?
   */
  private boolean isScreenLargerThan(final int width, final int height) {
    final DisplayMetrics displaymetrics = new DisplayMetrics();
    getWindowManager().getDefaultDisplay().getMetrics(displaymetrics);
    final int w = displaymetrics.widthPixels;
    final int h = displaymetrics.heightPixels;
    Log.d(getClass().getSimpleName(), "current screen: " + w + "x" + h);
    return w >= width && h >= height;
  }

  /*
   * Create all tabs for main options menu.
   */
  private void createTabs() {
    // Borderless buttons ?
    final boolean borderless = android.os.Build.VERSION.SDK_INT >= 11;

    // Create tabs
    final LinearLayout scroll = LinearLayout.class
        .cast(findViewById(R.id.layout));

    // Add all buttons linking to options
    for (int i = 0; i < tabClasses.length; i++) {
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
      final Class<?> cls = tabClasses[i];
      final Title title = Title.class.cast(cls.getAnnotation(Title.class));

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
      button.setText(title.value());

      // Set listener
      button.setOnClickListener(this);

      // Set tag to our index
      button.setTag(i);

      // And finally add button
      scroll.addView(button);
    }
  }

  /*
   * Set the view to the menu view
   */
  private void setViewMenu() {
    if (!isTabletMode) {
      // Set view
      setContentView(R.layout.activity_options);

      // Set Title
      setTitle(R.string.options);
    } else {
      // Set view
      setContentView(R.layout.activity_options_tablet);

      // Create left menu
      final ViewGroup leftContainer = ViewGroup.class
          .cast(findViewById(R.id.left));
      getLayoutInflater().inflate(R.layout.activity_options, leftContainer);

      // setPane(0);
    }

    // Create tabs
    createTabs();
  }

  @Override
  protected void onCreate(final Bundle savedInstanceState) {
    super.onCreate(savedInstanceState);

    // Large screen ? Enable special tablet features in such case...
    // "xlarge screens are at least 960dp x 720dp"
    isTabletMode = getResources().getBoolean(R.bool.isTablet)
        && isScreenLargerThan(800, 480);
    if (isTabletMode) {
      Log.d(getClass().getSimpleName(), "tablet mode detected");
    } else {
      Log.d(getClass().getSimpleName(), "phone mode detected");
    }

    // Load map
    map.unserialize(getIntent().getParcelableExtra("com.httrack.android.map"));
    Log.d(getClass().getSimpleName(), "map size: " + map.size());

    // Create tabs
    setViewMenu();
  }

  /*
   * Map getter.
   */
  protected String getMap(final int key) {
    return map.get(key);
  }

  /*
   * Map setter.
   */
  protected void setMap(final int key, final String value) {
    map.put(key, value);
  }

  @Override
  public void finish() {
    // Save all tabs
    Log.d(getClass().getSimpleName(), "final map size: " + map.size());

    // Declare result
    final Intent intent = new Intent();
    intent.putExtra("com.httrack.android.map", map);
    setResult(Activity.RESULT_OK, intent);
    super.finish();
  }

  /**
   * Override back to propagate settings. <br />
   * FIXME: we should probably do better than that!
   */
  @Override
  public void onBackPressed() {
    // Back from activity
    if (isTabletMode || activityClass == null) {
      if (isTabletMode) {
        save();
      }
      super.onBackPressed();
      finish();
    }
    // Leave sub-activity
    else {
      save();
      activityClass = null;
      setViewMenu();
    }
  }

  /*
   * Return current field ID's for the activity view.
   */
  private int[] getCurrentFields() {
    if (activityClass == null) {
      throw new RuntimeException("no current option selected");
    }
    final Fields fields = Fields.class.cast(activityClass
        .getAnnotation(Fields.class));
    if (fields == null) {
      throw new RuntimeException("no Fields annotation");
    }
    return fields.value();
  }

  /*
   * Return current title String ID for the activity view.
   */
  private int getCurrentActivityTitleId() {
    if (activityClass == null) {
      throw new RuntimeException("no current option selected");
    }
    final Title title = Title.class.cast(activityClass
        .getAnnotation(Title.class));
    if (title == null) {
      throw new RuntimeException("no Title annotation");
    }
    return title.value();
  }

  /*
   * Return current field ID's for the activity view.
   */
  private int getCurrentActivityId() {
    if (activityClass == null) {
      throw new RuntimeException("no current option selected");
    }
    final ActivityId activity = ActivityId.class.cast(activityClass
        .getAnnotation(ActivityId.class));
    if (activity == null) {
      throw new RuntimeException("no ActivityId annotation");
    }
    return activity.value();
  }

  /*
   * Load serialized field(s)
   */
  protected void load() {
    final int[] fields = getCurrentFields();
    Log.d(getClass().getSimpleName(), "loading " + fields.length + " fields");
    for (final int field : fields) {
      final String value = getMap(field);
      if (value != null) {
        widgetDataExchange.setFieldText(field, value);
      }
    }
  }

  /*
   * Serialize and save field(s)
   */
  protected void save() {
    final int[] fields = getCurrentFields();
    Log.d(getClass().getSimpleName(), "saving " + fields.length + " fields");
    for (final int field : fields) {
      final String value = widgetDataExchange.getFieldText(field);
      setMap(field, value);
    }
  }

  /*
   * Close the right pane in "tablet" view.
   */
  protected void closeRightPane() {
    // Remove right view ?
    if (isTabletMode && activityClass != null) {
      final ViewGroup group = ViewGroup.class.cast(findViewById(R.id.right));
      save();
      group.removeAllViews();
      activityClass = null;
    }
  }

  /*
   * Set pane #N (0..nb_panes).
   */
  protected void setPane(final int position) {
    // Close right pane if necessary
    closeRightPane();

    // Remove right view ?
    if (isTabletMode && activityClass != null) {
      final ViewGroup group = ViewGroup.class.cast(findViewById(R.id.right));
      save();
      group.removeAllViews();
      activityClass = null;
    }

    // Pickup corresponding class
    activityClass = tabClasses[position];

    // Set current view, current activity title, and load field(s)
    if (!isTabletMode) {
      setContentView(getCurrentActivityId());
    } else {
      final ViewGroup rightContainer = ViewGroup.class
          .cast(findViewById(R.id.right));
      getLayoutInflater().inflate(getCurrentActivityId(), rightContainer);
    }

    // Set new title
    setTitle(getCurrentActivityTitleId());

    // Load fields
    load();
  }

  @Override
  public void onClick(View v) {
    // Which button was clicked ?
    final Button cb = Button.class.cast(v);

    // Fetch tag position
    final int position = Integer.parseInt(cb.getTag().toString());

    // Set pane
    setPane(position);
  }
}
