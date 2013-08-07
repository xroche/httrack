# Copyright (C) 2009 The Android Open Source Project
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#      http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
#

# HTTrack library build: see prebuild/build.txt

LOCAL_PATH := $(call my-dir)

##$(shell (/cygdrive/c/Dev/android/android-ndk-r8e/ndk-build -C /cygdrive/c/Users/roche/android_workspace/HTTrack/jni))
##"C:\Program Files\cygwin\bin\bash.exe" --login -c "/cygdrive/c/Dev/android/android-ndk-r8e/ndk-build -C /cygdrive/c/Users/roche/android_workspace/HTTrack/jni"

include $(CLEAR_VARS)
LOCAL_MODULE    := libiconv
LOCAL_SRC_FILES := ../prebuild/libiconv.so
include $(PREBUILT_SHARED_LIBRARY)

include $(CLEAR_VARS)
LOCAL_MODULE    := libcrypto
LOCAL_SRC_FILES := ../prebuild/libcrypto.so
include $(PREBUILT_SHARED_LIBRARY)

include $(CLEAR_VARS)
LOCAL_MODULE    := libssl
LOCAL_SRC_FILES := ../prebuild/libssl.so
include $(PREBUILT_SHARED_LIBRARY)

include $(CLEAR_VARS)
LOCAL_MODULE    := libhttrack
LOCAL_SRC_FILES := ../prebuild/libhttrack.so
include $(PREBUILT_SHARED_LIBRARY)

include $(CLEAR_VARS)
LOCAL_MODULE    := libhtsjava
LOCAL_SRC_FILES := ../prebuild/libhtsjava.so
include $(PREBUILT_SHARED_LIBRARY)

include $(CLEAR_VARS)
LOCAL_MODULE    := htslibjni
LOCAL_SRC_FILES := htslibjni.c
LOCAL_C_INCLUDES := $(LOCAL_PATH)/../include
LOCAL_SHARED_LIBRARIES := libhttrack
#LOCAL_SHARED_LIBRARIES := libhttrack libhtsjava libiconv
#LOCAL_LDLIBS := -lhttrack
LOCAL_CFLAGS := -W -Wall -Wextra -Werror
include $(BUILD_SHARED_LIBRARY)
