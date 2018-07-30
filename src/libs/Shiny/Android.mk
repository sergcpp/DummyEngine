LOCAL_PATH := $(call my-dir)

########################################################################################################

include $(CLEAR_VARS)

LOCAL_MODULE     := Shiny
LOCAL_C_INCLUDES += $(GLOBAL_C_INCLUDES)
LOCAL_SRC_FILES  := src/_Shiny.c

include $(BUILD_STATIC_LIBRARY)

