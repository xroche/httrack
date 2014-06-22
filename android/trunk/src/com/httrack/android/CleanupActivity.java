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
import java.io.FilenameFilter;
import java.io.IOException;
import java.util.ArrayList;
import java.util.HashMap;
import java.util.HashSet;
import java.util.List;
import java.util.Map;

import android.app.Activity;
import android.app.AlertDialog;
import android.app.ListActivity;
import android.content.Context;
import android.content.DialogInterface;
import android.content.Intent;
import android.os.Bundle;
import android.util.Log;
import android.util.SparseArray;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.widget.CheckBox;
import android.widget.ListView;
import android.widget.SimpleAdapter;

import com.httrack.android.jni.HTTrackLib;

/**
 * Cleanup view.<br />
 * Thanks to David Silvera for the excellent related tutorial.
 */
public class CleanupActivity extends ListActivity {
  private ListView list;
  private File projectRootFile;
  private File resourceFile;
  private String[] projects;
  private int action;
  private final HashSet<Integer> toBeDeleted = new HashSet<Integer>();

  public static final int ACTION_CLEANUP = 1;
  public static final int ACTION_SELECT = 2;

  /**
   * List adapter.
   */
  public class CleanupListAdapter extends SimpleAdapter {
    private final LayoutInflater inflater;

    public CleanupListAdapter(final Context context,
        final List<? extends Map<String, ?>> data, final int resource, final String[] from,
        final int[] to) {
      super(context, data, resource, from, to);
      inflater = LayoutInflater.from(context);
    }

    @Override
    public Object getItem(final int position) {
      return super.getItem(position);
    }

    @Override
    public View getView(final int position, View convertView, final ViewGroup parent) {
      if (convertView == null) {
        convertView = inflater.inflate(R.layout.cleanup_item, null);
        final CheckBox cb = (CheckBox) convertView.findViewById(R.id.check);
        if (cb != null) {
          cb.setTag(position);
        }
      }
      return super.getView(position, convertView, parent);
    }

  }

  /** Called when the activity is first created. */
  @Override
  public void onCreate(final Bundle savedInstanceState) {
    super.onCreate(savedInstanceState);
    setContentView(R.layout.activity_cleanup);
    list = getListView();

    // Fetch args from parent
    final Bundle extras = getIntent().getExtras();
    projectRootFile = File.class.cast(extras
        .get("com.httrack.android.rootFile"));
    resourceFile = File.class.cast(extras
        .get("com.httrack.android.resourceFile"));
    projects = (String[]) extras.get("com.httrack.android.projectNames");
    action = extras.getInt("com.httrack.android.action");

    if (projectRootFile == null || resourceFile == null || projects == null
        || action == 0) {
      throw new RuntimeException("internal error");
    }
    
    // Visibility of "delete" and its hline.
    final int state = action == ACTION_CLEANUP ? View.VISIBLE : View.GONE;
    findViewById(R.id.buttonClear).setVisibility(state);
    findViewById(R.id.horizontalLine).setVisibility(state);

    final ArrayList<HashMap<String, String>> listItem = new ArrayList<HashMap<String, String>>();

    for (final String name : projects) {
      final HashMap<String, String> map = new HashMap<String, String>();
      map.put("name", name);

      // Map used to lookup projects
      final SparseArray<String> projectMap = new SparseArray<String>();
      final File target = new File(projectRootFile, name);
      try {
        OptionsMapper.unserialize(HTTrackActivity.getProfileFile(target),
            projectMap);
        final String description = projectMap.get(R.id.fieldWebsiteURLs);
        map.put("description", description);
      } catch (final IOException e) {
        map.put("description", name);
      }

      // Add item
      listItem.add(map);
    }

    final CleanupListAdapter adapter = new CleanupListAdapter(
        this.getBaseContext(), listItem, R.layout.cleanup_item, new String[] {
            "name", "description" }, new int[] { R.id.name, R.id.description });
    list.setAdapter(adapter);
  }

  public void OnClickCheckbox(final View v) {
    final CheckBox cb = (CheckBox) v;
    final int position = Integer.parseInt(cb.getTag().toString());
    final View o = list.getChildAt(position).findViewById(R.id.blocCheck);

    if (action == ACTION_CLEANUP) {
      if (cb.isChecked()) {
        o.setBackgroundResource(R.color.transparent_red);
        toBeDeleted.add(position);
      } else {
        o.setBackgroundResource(R.color.transparent);
        toBeDeleted.remove(position);
      }
    } else if (action == ACTION_SELECT) {
      // Push result
      final Intent intent = new Intent();
      intent.putExtra("com.httrack.android.projectName", projects[position]);
      setResult(Activity.RESULT_OK, intent);

      // Finish activity
      finish();
    }
  }

  /* Delete recursively a directory, or delete a file. */
  public static boolean deleteRecursively(final File file) {
    // TODO: check if this is necessary (symbolic link handling to avoid
    // infinite loops)
    if (file.delete()) {
      return true;
    } else {
      if (file.isDirectory()) {
        for (final File child : file.listFiles()) {
          deleteRecursively(child);
        }
      }
      return file.delete();
    }
  }

  /**
   * Is this path empty ? Note: ignores .nomedia files.
   * 
   * @return true if the root path is empty.
   */
  public static boolean pathIsEmpty(final File projectRootFile) {
    return projectRootFile != null
        && projectRootFile.listFiles(new FilenameFilter() {
          @Override
          public boolean accept(final File dir, final String filename) {
            return !filename.equals(HTTrackActivity.NOMEDIA_FILE);
          }
        }).length != 0;
  }

  protected boolean deleteProjects() {
    boolean success = true;
    for (final int position : toBeDeleted) {
      // Delete project.
      final String name = projects[position];
      final File target = new File(projectRootFile, name);
      if (deleteRecursively(target)) {
        // Mark item as disabled.
        final View item = list.getChildAt(position);
        final View o = item.findViewById(R.id.blocCheck);
        o.setBackgroundResource(R.color.gray);
        final CheckBox cb = (CheckBox) item.findViewById(R.id.check);
        cb.setClickable(false);
        cb.setEnabled(false);
      } else {
        success = false;
      }

    }
    // Everything was deleted
    toBeDeleted.clear();

    // Root path can be deleted ?
    final boolean deleteRootPath = pathIsEmpty(projectRootFile);
    if (deleteRootPath) {
      if (deleteRecursively(projectRootFile)) {
        Log.d(getClass().getSimpleName(), "successfully deleted root path: "
            + projectRootFile);
      } else {
        Log.w(getClass().getSimpleName(), "could not delet root path: "
            + projectRootFile);
      }

      // Delete top-level parent if a "HTTrack" folder was found. This ensure
      // that we fully cleanup user-data.
      final File parentRoot = projectRootFile.getParentFile();
      if (parentRoot != null && parentRoot.getName().equals("HTTrack")
          && pathIsEmpty(parentRoot)) {
        if (deleteRecursively(parentRoot)) {
          Log.d(getClass().getSimpleName(), "successfully deleted root path: "
              + parentRoot);
        } else {
          Log.w(getClass().getSimpleName(), "could not delet root path: "
              + parentRoot);
        }
      }
    } else {
      // Rebuild top index
      HTTrackLib.buildTopIndex(projectRootFile, resourceFile);
    }

    // Push result
    final Intent intent = new Intent();
    intent.putExtra("com.httrack.android.rootPathWasDeleted", deleteRootPath);
    setResult(Activity.RESULT_OK, intent);

    // Finish
    finish();

    return success;
  }

  public void onClickDelete(final View v) {
    if (action == ACTION_CLEANUP) {
      String projectList = "";
      for (final int position : toBeDeleted) {
        final String name = projects[position];
        projectList += "\n";
        projectList += name;
      }
      if (projectList.length() != 0) {
        new AlertDialog.Builder(this)
            .setIcon(android.R.drawable.ic_dialog_alert)
            .setTitle("Delete Projects")
            .setMessage(
                "Are you sure you want to delete selected projects ?"
                    + projectList)
            .setPositiveButton("Yes", new DialogInterface.OnClickListener() {
              @Override
              public void onClick(final DialogInterface dialog, final int which) {
                deleteProjects();
              }
            }).setNegativeButton("No", null).show();
      }
    }
  }
}
