LOCAL_PATH := $(call my-dir)

include $(CLEAR_VARS)

LOCAL_MODULE    := runner 
LOCAL_C_INCLUDES := easyvk/src/ easyvk/volk
LOCAL_SRC_FILES := easyvk/src/easyvk.cpp runner.cpp
LOCAL_LDLIBS    += -lvulkan -llog

include $(BUILD_EXECUTABLE)
