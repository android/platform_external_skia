LOCAL_PATH:= $(call my-dir)

include $(CLEAR_VARS)

LOCAL_CFLAG := -Wall

LOCAL_SRC_FILES := fontrec.c

LOCAL_MODULE_TAGS := optional
LOCAL_MODULE := libfontrec
LOCAL_PRELINK_MODULE := false

include $(BUILD_SHARED_LIBRARY)
