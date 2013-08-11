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

import android.app.Activity;
import android.view.View;
import android.widget.CheckBox;
import android.widget.RadioGroup;
import android.widget.TextView;

/**
 * Helper aimed for data exchange between widgets (TextView, radio, etc.)
 */
public class WidgetDataExchange {
  protected final Activity parentView;

  /**
   * Constructor.
   *
   * @param view
   *          The parent view.
   */
  public WidgetDataExchange(final Activity view) {
    this.parentView = view;
  }

  /*
   * Set text field.
   */
  public void setFieldText(final int id, final String value) {
    final View view = parentView.findViewById(id);
    if (view == null) {
      throw new RuntimeException("no such view " + Integer.toString(id));
    }
    if (view instanceof CheckBox) {
      final CheckBox checkbox = (CheckBox) view;
      checkbox.setChecked("1".equalsIgnoreCase(value));
    } else if (view instanceof RadioGroup) {
      if (value != null && value.length() != 0) {
        final int selected = Integer.parseInt(value);
        final RadioGroup radio = (RadioGroup) view;
        final View child = radio.getChildAt(selected);
        if (child == null) {
          // Ignore
          return;
        }
        radio.check(child.getId());
      }
    } else if (view instanceof TextView) {
      if (value != null) {
        final TextView text = (TextView) view;
        text.setText(value);
      }
    } else {
      throw new RuntimeException("unsupported class "
          + view.getClass().getName() + " with setFieldText");
    }
  }

  /*
   * Get field text
   */
  public String getFieldText(final int id) {
    final View view = parentView.findViewById(id);
    if (view == null) {
      throw new RuntimeException("no such view " + Integer.toString(id));
    }
    if (view instanceof CheckBox) {
      final CheckBox checkbox = (CheckBox) view;
      return checkbox.isChecked() ? "1" : "0";
    } else if (view instanceof TextView) {
      final TextView text = (TextView) view;
      return text.getText().toString();
    } else if (view instanceof RadioGroup) {
      final RadioGroup radio = (RadioGroup) view;
      final int checkedId = radio.getCheckedRadioButtonId();
      if (checkedId != -1) {
        for (int i = 0; i < radio.getChildCount(); i++) {
          if (checkedId == radio.getChildAt(i).getId()) {
            return Integer.toString(i);
          }
        }
        throw new RuntimeException("unexpected RadioGroup error");
      } else {
        return null;
      }
    } else {
      throw new RuntimeException("unsupported class "
          + view.getClass().getName() + " with getFieldText");
    }
  }

}
