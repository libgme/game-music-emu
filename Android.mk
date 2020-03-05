# Support for Android-based projects that use ndk-build instead of CMake, such
# as LÖVE
LOCAL_PATH := $(call my-dir)

include $(CLEAR_VARS)

LOCAL_MODULE    := libgme
FILE_LIST	:= $(wildcard $(LOCAL_PATH)/gme/*.cpp)
LOCAL_SRC_FILES := $(FILE_LIST:$(LOCAL_PATH)/%=%)
LOCAL_CPPFLAGS := -Wall -W -Wextra -std=c++11 -O2 -DBLARGG_BUILD_DLL \
	-DLIBGME_VISIBILITY -fvisibility=hidden -fvisibility-inlines-hidden \
	-fwrapv -DVGM_YM2612_GENS
LOCAL_SANITIZE := undefined
LOCAL_CPP_FEATURES := exceptions

include $(BUILD_SHARED_LIBRARY)
