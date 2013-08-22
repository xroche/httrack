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

include $(CLEAR_VARS)
LOCAL_MODULE    := libiconv
LOCAL_SRC_FILES := ../prebuild/libiconv.so
include $(PREBUILT_SHARED_LIBRARY)

# TODO FIXME: INVESTIGATE THIS
# Android seems to have both /system/lib/libcrypto.so and
# /system/lib/libssl.so, and Java loads them in priority when using
# System.loadLibrary(...)
# We should either ensure that the system libs are fine (they seems fine BTW)
# or explicitly load the application-cached-library using some dirty magic
# (ie. giving the explicit full path and .so)
# For now, we do not ship the libraries, which will spare some space, and
# hope the ABI won't change too much.

# include $(CLEAR_VARS)
# LOCAL_MODULE    := libcrypto
# LOCAL_SRC_FILES := ../prebuild/libcrypto.so
# include $(PREBUILT_SHARED_LIBRARY)

# include $(CLEAR_VARS)
# LOCAL_MODULE    := libssl
# LOCAL_SRC_FILES := ../prebuild/libssl.so
# include $(PREBUILT_SHARED_LIBRARY)

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
LOCAL_SRC_FILES := htslibjni.c coffeecatch.c
LOCAL_C_INCLUDES := $(LOCAL_PATH)/../include
LOCAL_SHARED_LIBRARIES := libhttrack
LOCAL_CFLAGS := -W -Wall -Wextra -Werror
include $(BUILD_SHARED_LIBRARY)
