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
  public void setFieldText(int id, String value) {
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
  public String getFieldText(int id) {
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
