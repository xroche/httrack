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

import android.net.Uri;
import android.os.Bundle;
import android.app.Activity;
import android.content.Intent;
import android.view.Menu;
import android.view.View;

public class HelpActivity extends Activity {
  protected File resourceFile;

  @Override
  protected void onCreate(Bundle savedInstanceState) {
    super.onCreate(savedInstanceState);
    setContentView(R.layout.activity_help);

    final Bundle extras = getIntent().getExtras();
    resourceFile = File.class.cast(extras.get("resourceFile"));
  }

  @Override
  public boolean onCreateOptionsMenu(Menu menu) {
    // Inflate the menu; this adds items to the action bar if it is present.
    getMenuInflater().inflate(R.menu.help, menu);
    return true;
  }

  /** Browser a specific index. **/
  private void browse(final File index) {
    final Intent intent = HTTrackActivity.getBrowseIntent(index);
    if (intent != null) {
      startActivity(intent);
    }
  }

  /** Browser a specific web page. **/
  private void browse(final Uri uri) {
    final Intent intent = new Intent();
    intent.setAction(android.content.Intent.ACTION_VIEW);
    intent.setData(uri);
    startActivity(intent);
  }

  /**
   * "View Documentation"
   */
  public void onClickViewDoc(final View view) {
    browse(new File(new File(resourceFile, "html"), "index.html"));
  }

  /**
   * "Browse HTTrack Site"
   */
  public void onClickBrowseHTTrackSite(final View view) {
    browse(Uri.parse("http://www.httrack.com/"));
  }

  /**
   * "Browse HTTrack Forum"
   */
  public void onClickBrowseHTTrackForum(final View view) {
    browse(Uri.parse("http://forum.httrack.com/"));
  }
}
