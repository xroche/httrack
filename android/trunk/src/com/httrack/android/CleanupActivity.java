package com.httrack.android;

import java.io.File;
import java.io.IOException;
import java.util.ArrayList;
import java.util.HashMap;
import java.util.HashSet;
import java.util.List;
import java.util.Map;

import com.httrack.android.jni.HTTrackLib;

import android.app.AlertDialog;
import android.app.ListActivity;
import android.content.Context;
import android.content.DialogInterface;
import android.os.Bundle;
import android.util.SparseArray;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.widget.CheckBox;
import android.widget.ListView;
import android.widget.SimpleAdapter;

/**
 * Cleanup view.<br />
 * Thanks to David Silvera for the excellent related tutorial.
 */
public class CleanupActivity extends ListActivity {
  private ListView list;
  private File projectRootFile;
  private File resourceFile;
  private String[] projects;
  private final HashSet<Integer> toBeDeleted = new HashSet<Integer>();

  /**
   * List adapter.
   */
  public class CleanupListAdapter extends SimpleAdapter {
    private LayoutInflater inflater;

    public CleanupListAdapter(Context context,
        List<? extends Map<String, ?>> data, int resource, String[] from,
        int[] to) {
      super(context, data, resource, from, to);
      inflater = LayoutInflater.from(context);
    }

    @Override
    public Object getItem(int position) {
      return super.getItem(position);
    }

    @Override
    public View getView(int position, View convertView, ViewGroup parent) {
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
    projectRootFile = File.class.cast(getIntent().getExtras().get("rootFile"));
    resourceFile = File.class.cast(getIntent().getExtras().get("resourceFile"));
    projects = (String[]) getIntent().getExtras().get("projectNames");
    if (projectRootFile == null || resourceFile == null || projects == null) {
      throw new RuntimeException("internal error");
    }

    final ArrayList<HashMap<String, String>> listItem = new ArrayList<HashMap<String, String>>();

    for (String name : projects) {
      final HashMap<String, String> map = new HashMap<String, String>();
      map.put("name", name);

      // Map used to lookup projects
      final SparseArray<String> projectMap = new SparseArray<String>();
      final File target = new File(projectRootFile, name);
      try {
        HTTrackActivity.unserialize(HTTrackActivity.getProfileFile(target),
            projectMap);
        final String description = projectMap.get(R.id.fieldWebsiteURLs);
        map.put("description", description);
      } catch (IOException e) {
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
    if (cb.isChecked()) {
      o.setBackgroundResource(R.color.transparent_red);
      toBeDeleted.add(position);
    } else {
      o.setBackgroundResource(R.color.transparent);
      toBeDeleted.remove(position);
    }
  }

  /* Delete recursively a directory, or delete a file. */
  private boolean deleteRecursively(final File file) {
    // TODO: check if this is necessary (symbolic link handling to avoid
    // infinite loops)
    if (file.delete()) {
      return true;
    } else {
      if (file.isDirectory()) {
        for (File child : file.listFiles()) {
          deleteRecursively(child);
        }
      }
      return file.delete();
    }
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

    // Rebuild top index
    HTTrackLib.buildTopIndex(projectRootFile, resourceFile);

    return success;
  }

  public void onClickDelete(final View v) {
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
            public void onClick(DialogInterface dialog, int which) {
              deleteProjects();
            }
          }).setNegativeButton("No", null).show();
    }
  }
}
