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
import android.view.Menu;
import android.widget.TabHost;
import android.widget.TabHost.TabSpec;

/**
 * The tab host activity.<br />
 * Many thanks to Pierre-Emmanuel Mercier for his nice tutorial.
 */
public class OptionsActivity extends TabActivity {
  @SuppressWarnings("unchecked")
  protected static Class<? extends Tab>[] tabClasses = new Class[] {
      ScanRulesTab.class, LimitsTab.class, FlowControlTab.class, LinksTab.class };

  private TabHost tabHost;
  private final List<TabSpec> tabSpec = new ArrayList<TabSpec>();

  /**
   * The tab activit(ies) common class.
   */
  public abstract static class Tab extends Activity {
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
    protected void onCreate(final Bundle savedInstanceState) {
      super.onCreate(savedInstanceState);
      setContentView(R.layout.activity_options_scanrules);
    }
  }

  @Title("Limits")
  public static class LimitsTab extends Tab {
    @Override
    protected void onCreate(final Bundle savedInstanceState) {
      super.onCreate(savedInstanceState);
      setContentView(R.layout.activity_options_limits);
    }
  }

  @Title("Flow Control")
  public static class FlowControlTab extends Tab {
    @Override
    protected void onCreate(final Bundle savedInstanceState) {
      super.onCreate(savedInstanceState);
      setContentView(R.layout.activity_options_flowcontrol);
    }
  }

  @Title("Links")
  public static class LinksTab extends Tab {
    @Override
    protected void onCreate(final Bundle savedInstanceState) {
      super.onCreate(savedInstanceState);
      setContentView(R.layout.activity_options_links);
    }
  }

  @Override
  protected void onCreate(final Bundle savedInstanceState) {
    super.onCreate(savedInstanceState);
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

}
