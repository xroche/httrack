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
import android.util.Log;
import android.view.Gravity;
import android.view.View;
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
      Proxy.class, LogIndexCache.class, ExpertsOnly.class };

  // The parent map
  protected final SparseArraySerializable map = new SparseArraySerializable();

  // Widget data exchanger
  private final WidgetDataExchange widgetDataExchange = new WidgetDataExchange(
      this);

  // Current activity class
  protected Class<?> activityClass;

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
  @Fields({ R.id.editBrowserIdentity, R.id.editHtmlFooter })
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
      R.id.radioVerbosity, R.id.checkUseIndex })
  public static class LogIndexCache implements Tab {
  }

  @Title(R.string.experts_only)
  @ActivityId(R.layout.activity_options_expertsonly)
  @Fields({ R.id.checkUseCacheForUpdates, R.id.radioPrimaryScanRule,
      R.id.textTravelMode, R.id.radioTravelMode, R.id.radioGlobalTravelMode,
      R.id.radioRewriteLinks, R.id.checkActivateDebugging })
  public static class ExpertsOnly implements Tab {
  }

  /*
   * Create all tabs
   */
  private void createTabs() {
    // Create tabs
    final LinearLayout scroll = LinearLayout.class
        .cast(findViewById(R.id.layout));
    for (int i = 0; i < tabClasses.length; i++) {
      final Class<?> cls = tabClasses[i];
      final Title title = Title.class.cast(cls.getAnnotation(Title.class));
      // Create a flat button ; see
      // <http://developer.android.com/guide/topics/ui/controls/button.html#Borderless>
      // Added in API level 11
      final Button button = android.os.Build.VERSION.SDK_INT >= 11 ? new Button(
          this, null, android.R.attr.borderlessButtonStyle) : new Button(this);
      // Left-aligned text
      if (android.os.Build.VERSION.SDK_INT >= 11) {
        button.setGravity(Gravity.LEFT | Gravity.CENTER_VERTICAL);
      }
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
    // Set view
    setContentView(R.layout.activity_options);

    // Set Title
    setTitle(R.string.options);

    // Create tabs
    createTabs();
  }

  @Override
  protected void onCreate(final Bundle savedInstanceState) {
    super.onCreate(savedInstanceState);

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
    if (activityClass == null) {
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

  @Override
  public void onClick(View v) {
    // Which button was clicked ?
    final Button cb = Button.class.cast(v);

    // Fetch tag position
    final int position = Integer.parseInt(cb.getTag().toString());

    // Pickup corresponding class
    activityClass = tabClasses[position];

    // Set current view, current activity title, and load field(s)
    setContentView(getCurrentActivityId());
    setTitle(getCurrentActivityTitleId());
    load();
  }
}
