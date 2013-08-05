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
import java.util.ArrayList;
import java.util.List;

import android.os.Bundle;
import android.app.Activity;
import android.app.TabActivity;
import android.content.Intent;
import android.util.Log;
import android.view.Menu;
import android.widget.TabHost;
import android.widget.TabHost.TabSpec;

/**
 * The tab host activity.<br />
 * Many thanks to Pierre-Emmanuel Mercier for his nice tutorial.
 */
public class OptionsActivity extends TabActivity {
  /* List of all tabs. */
  @SuppressWarnings("unchecked")
  protected static Class<? extends Tab>[] tabClasses = new Class[] {
      ScanRulesTab.class, LimitsTab.class, FlowControlTab.class,
      LinksTab.class, BuildTab.class, BrowserId.class, Spider.class,
      Proxy.class, LogIndexCache.class, ExpertsOnly.class };

  protected final SparseArraySerializable map = new SparseArraySerializable();
  private TabHost tabHost;
  private final List<TabSpec> tabSpec = new ArrayList<TabSpec>();

  /**
   * The tab activit(ies) common class.
   */
  public abstract static class Tab extends Activity {
    protected OptionsActivity parentOptions;
    private WidgetDataExchange widgetDataExchange = new WidgetDataExchange(this);

    @Override
    protected void onCreate(final Bundle savedInstanceState) {
      super.onCreate(savedInstanceState);
      parentOptions = OptionsActivity.class.cast(getParent());
      if (parentOptions == null) {
        throw new RuntimeException("tab has no parent!");
      }
    }

    /**
     * List of fields.
     * 
     * @return
     */
    protected int[] getFields() {
      return new int[] {};
    }

    /*
     * Load serialized field(s)
     */
    protected void load() {
      final int[] fields = getFields();
      Log.d(this.getClass().getName(), "loading " + fields.length + " fields");
      for (final int field : fields) {
        final String value = parentOptions.getMap(field);
        if (value != null) {
          widgetDataExchange.setFieldText(field, value);
        }
      }
    }

    /*
     * Serialize and save field(s)
     */
    protected void save() {
      final int[] fields = getFields();
      Log.d(this.getClass().getName(), "saving " + fields.length + " fields");
      for (final int field : fields) {
        final String value = widgetDataExchange.getFieldText(field);
        parentOptions.setMap(field, value);
      }
    }

    @Override
    public void setContentView(int layoutResID) {
      super.setContentView(layoutResID);
      load();
    }

    @Override
    public void finish() {
      save();
      super.finish();
    }
  }

  /**
   * Title of a tab class.
   */
  @Retention(RetentionPolicy.RUNTIME)
  @Target(ElementType.TYPE)
  public static @interface Title {
    String value();

  }

  @Title("Scan Rules")
  public static class ScanRulesTab extends Tab {
    @Override
    protected int[] getFields() {
      return new int[] { R.id.editRules };
    }

    @Override
    protected void onCreate(final Bundle savedInstanceState) {
      super.onCreate(savedInstanceState);
      setContentView(R.layout.activity_options_scanrules);
    }
  }

  @Title("Limits")
  public static class LimitsTab extends Tab {
    @Override
    protected int[] getFields() {
      return new int[] { R.id.editMaxDepth, R.id.editMaxExtDepth,
          R.id.editMaxSizeHtml, R.id.editMaxSizeOther, R.id.editSiteSizeLimit,
          R.id.editMaxTimeOverall, R.id.editMaxTransferRate,
          R.id.editMaxConnectionsSecond, R.id.editMaxNumberLinks };
    }

    @Override
    protected void onCreate(final Bundle savedInstanceState) {
      super.onCreate(savedInstanceState);
      setContentView(R.layout.activity_options_limits);
    }
  }

  @Title("Flow Control")
  public static class FlowControlTab extends Tab {
    @Override
    protected int[] getFields() {
      return new int[] { R.id.editNumberOfConnections,
          R.id.checkPersistentConnections, R.id.editTimeout,
          R.id.checkRemoveHostIfTimeout, R.id.editRetries,
          R.id.editMinTransferRate, R.id.checkRemoveHostIfSlow };
    }

    @Override
    protected void onCreate(final Bundle savedInstanceState) {
      super.onCreate(savedInstanceState);
      setContentView(R.layout.activity_options_flowcontrol);
    }
  }

  @Title("Links")
  public static class LinksTab extends Tab {
    @Override
    protected int[] getFields() {
      return new int[] { R.id.checkDetectAllLinks, R.id.checkGetNonHtmlNear,
          R.id.checkTestAllLinks, R.id.checkGetHtmlFirst };
    }

    @Override
    protected void onCreate(final Bundle savedInstanceState) {
      super.onCreate(savedInstanceState);
      setContentView(R.layout.activity_options_links);
    }
  }

  @Title("Build")
  public static class BuildTab extends Tab {
    @Override
    protected int[] getFields() {
      return new int[] { R.id.checkDosNames, R.id.checkIso9660,
          R.id.checkNoErrorPages, R.id.checkNoExternalPages,
          R.id.checkHidePasswords, R.id.checkHideQueryStrings,
          R.id.checkDoNotPurge, R.id.radioBuild, R.id.editCustomBuild };
    }

    @Override
    protected void onCreate(final Bundle savedInstanceState) {
      super.onCreate(savedInstanceState);
      setContentView(R.layout.activity_options_build);
    }
  }

  @Title("Browser ID")
  public static class BrowserId extends Tab {
    @Override
    protected int[] getFields() {
      return new int[] { R.id.editBrowserIdentity, R.id.editHtmlFooter };
    }

    @Override
    protected void onCreate(final Bundle savedInstanceState) {
      super.onCreate(savedInstanceState);
      setContentView(R.layout.activity_options_browserid);
    }
  }

  @Title("Spider")
  public static class Spider extends Tab {
    @Override
    protected int[] getFields() {
      return new int[] { R.id.checkAcceptCookies, R.id.radioCheckDocumentType,
          R.id.checkParseJavaFiles, R.id.radioSpider, R.id.checkUpdateHacks,
          R.id.checkUrlHacks, R.id.checkTolerentRequests, R.id.checkForceHttp10 };
    }

    @Override
    protected void onCreate(final Bundle savedInstanceState) {
      super.onCreate(savedInstanceState);
      setContentView(R.layout.activity_options_spider);
    }
  }

  @Title("Proxy")
  public static class Proxy extends Tab {
    @Override
    protected int[] getFields() {
      return new int[] { R.id.editProxy, R.id.editProxyPort,
          R.id.checkUseProxyForFtp };
    }

    @Override
    protected void onCreate(final Bundle savedInstanceState) {
      super.onCreate(savedInstanceState);
      setContentView(R.layout.activity_options_proxy);
    }
  }

  @Title("Log, Index, Cache")
  public static class LogIndexCache extends Tab {
    @Override
    protected int[] getFields() {
      return new int[] { R.id.checkStoreAllFilesInCache,
          R.id.checkDoNotRedownloadLocallErasedFiles, R.id.checkCreateLogFiles,
          R.id.radioVerbosity, R.id.checkUseIndex };
    }

    @Override
    protected void onCreate(final Bundle savedInstanceState) {
      super.onCreate(savedInstanceState);
      setContentView(R.layout.activity_options_logindexcache);
    }
  }

  @Title("Experts Only")
  public static class ExpertsOnly extends Tab {
    @Override
    protected int[] getFields() {
      return new int[] { R.id.checkUseCacheForUpdates,
          R.id.radioPrimaryScanRule, R.id.textTravelMode, R.id.radioTravelMode,
          R.id.radioGlobalTravelMode, R.id.radioRewriteLinks,
          R.id.checkActivateDebugging };
    }

    @Override
    protected void onCreate(final Bundle savedInstanceState) {
      super.onCreate(savedInstanceState);
      setContentView(R.layout.activity_options_expertsonly);
    }
  }

  @Override
  protected void onCreate(final Bundle savedInstanceState) {
    super.onCreate(savedInstanceState);

    // Load map
    map.unserialize(getIntent().getParcelableExtra("map"));
    Log.d(this.getClass().getName(), "map size: " + map.size());

    // Set view
    setContentView(R.layout.activity_options);

    // Create tabs
    tabHost = getTabHost();
    for (final Class<? extends Tab> cls : tabClasses) {
      final Title title = Title.class.cast(cls.getAnnotation(Title.class));
      tabSpec.add(tabHost.newTabSpec(Integer.toString(tabSpec.size()))
          .setIndicator(title.value()).setContent(new Intent(this, cls)));
    }

    // Populate tab
    for (final TabSpec tab : tabSpec) {
      tabHost.addTab(tab);
    }
  }

  @Override
  public boolean onCreateOptionsMenu(final Menu menu) {
    // Inflate the menu; this adds items to the action bar if it is present.
    getMenuInflater().inflate(R.menu.options, menu);
    return true;
  }

  /*
   * Map getter.
   */
  protected String getMap(int key) {
    return map.get(key);
  }

  /*
   * Map setter.
   */
  protected void setMap(int key, final String value) {
    map.put(key, value);
  }

  @Override
  public void finish() {
    // Save all tabs
    Log.d(this.getClass().getName(), "final map size: " + map.size());

    // Declare result
    final Intent intent = new Intent();
    intent.putExtra("map", map);
    setResult(Activity.RESULT_OK, intent);
    super.finish();
  }

  /**
   * Override back to propagate settings. <br />
   * FIXME: we should probably do better than that!
   */
  @Override
  public void onBackPressed() {
    super.onBackPressed();
    finish();
  }
}
