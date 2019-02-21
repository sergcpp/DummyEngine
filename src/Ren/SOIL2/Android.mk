LOCAL_PATH := $(call my-dir)

include $(CLEAR_VARS)

LOCAL_MODULE     := SOIL2
LOCAL_SRC_FILES  := etc1_utils.c	\
                    image_DXT.c		\
                    image_helper.c	\
                    SOIL2.c

LOCAL_LDLIBS    := -lEGL

include $(BUILD_STATIC_LIBRARY)

