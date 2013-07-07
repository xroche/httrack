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

# HTTrack library build: (TEMPORARY)
#
# rm -rf /tmp/httrack-3.47.19 && cp -ax ~/svn/httrack/trunk /tmp/httrack-3.47.19 && cd /tmp/httrack-3.47.19
# rm -rf /tmp/arm-inst
# ./configure -host=arm-none-linux \
#  --prefix=/tmp/arm-inst \
#  --enable-shared --disable-static \
#  --with-sysroot=/home/roche/bin/android-ndk-r8e/platforms/android-5/arch-arm \
#  --includedir=/home/roche/bin/android-ndk-r8e/platforms/android-5/arch-arm/usr/include \
#  --disable-https \
#  CC="/home/roche/bin/android-ndk-r8e/toolchains/arm-linux-androideabi-4.6/prebuilt/linux-x86_64/bin/arm-linux-androideabi-gcc --sysroot=/home/roche/bin/android-ndk-r8e/platforms/android-5/arch-arm -I/home/roche/bin/android-ndk-r8e/platforms/android-5/arch-arm/usr/include -L/home/roche/bin/android-ndk-r8e/platforms/android-5/arch-arm/usr/lib" \
#  CFLAGS="-DHTS_USEICONV=0 -DANDROID -D_ANDROID" \
#  && sed -i -e 's/CPPFLAGS = .*/CPPFLAGS =/' Makefile \
#  && sed -i -e 's/CPPFLAGS = .*/CPPFLAGS =/' src/Makefile \
#  && find . -name Makefile -exec sed -i -e "s/\(.*LDFLAGS = \)-version-info .*/\1 -avoid-version/" {} \; \
#  && make -j8 && make install DESTDIR=/tmp/arm-inst \
#  && make install

# JNI library build:
#
# cd /cygdrive/c/Users/roche/android_workspace/HTTrack/jni
# /cygdrive/c/Dev/android/android-ndk-r8e/ndk-build

LOCAL_PATH := $(call my-dir)

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
#LOCAL_SHARED_LIBRARIES := libhttrack libhtsjava
#LOCAL_LDLIBS := -lhttrack
include $(BUILD_SHARED_LIBRARY)
