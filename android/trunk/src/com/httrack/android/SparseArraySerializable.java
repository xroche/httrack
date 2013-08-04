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

import android.os.Parcel;
import android.os.Parcelable;
import android.util.SparseArray;

/**
 * A Parcelable (serializable) string sparse array
 */
public class SparseArraySerializable extends SparseArray<String> implements
    Parcelable {

  public SparseArraySerializable() {
    super();
  }

  public SparseArraySerializable(final Parcel in) {
    final int size = in.readInt();
    for (int i = 0; i < size; i++) {
      final int key = in.readInt();
      final String value = in.readString();
      put(key, value);
    }
  }

  /**
   * Merge an existing SparseArray
   * 
   * @param other
   *          the other SparseArray
   */
  public void fill(final SparseArray<String> other) {
    final int size = other.size();
    for (int i = 0; i < size; i++) {
      final int key = other.keyAt(i);
      final String value = other.valueAt(i);
      put(key, value);
    }
  }

  /**
   * Unserialize the sparse array.
   * 
   * @param object
   *          The serialized object.
   */
  public void unserialize(final Parcelable object) {
    @SuppressWarnings("unchecked")
    final SparseArray<String> newMap = (SparseArray<String>) object;
    if (newMap == null) {
      throw new RuntimeException("map is null");
    }
    clear();
    fill(newMap);
  }

  @Override
  public int describeContents() {
    return 0;
  }

  @Override
  public void writeToParcel(final Parcel dest, int flags) {
    final int size = size();
    dest.writeInt(size);
    for (int i = 0; i < size; i++) {
      dest.writeInt(keyAt(i));
      dest.writeString(valueAt(i));
    }
  }

  public static final Parcelable.Creator<SparseArraySerializable> CREATOR = new Parcelable.Creator<SparseArraySerializable>() {
    public SparseArraySerializable createFromParcel(final Parcel in) {
      return new SparseArraySerializable(in);
    }

    public SparseArraySerializable[] newArray(int size) {
      return new SparseArraySerializable[size];
    }
  };
}
