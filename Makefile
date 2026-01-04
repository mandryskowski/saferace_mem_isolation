CXXFLAGS = -std=c++17 -DVK_USE_PLATFORM_WIN32_KHR
CLSPVFLAGS = --cl-std=CL2.0 --spv-version=1.3 --inline-entry-points

VULKAN_INCLUDE = -I"$(VULKAN_SDK)/Include"
VULKAN_LIB = -L"$(VULKAN_SDK)/Lib"

ALL_SHADERS = $(wildcard *.cl)
IGNORE := phys-device-hack.cl
SHADERS := $(filter-out $(IGNORE), $(ALL_SHADERS))
SPVS = $(patsubst %.cl,%.spv,$(SHADERS))

.PHONY: all build clean easyvk runner 

all: build easyvk runner

build:
	mkdir -p build
	mkdir -p build/android

easyvk: easyvk/src/easyvk.cpp easyvk/src/easyvk.h
	$(CXX) $(CXXFLAGS) $(VULKAN_INCLUDE) -Ieasyvk/src -c easyvk/src/easyvk.cpp -o build/easyvk.o

runner: easyvk runner.cpp $(SPVS)
	$(CXX) $(CXXFLAGS) $(VULKAN_INCLUDE) -Ieasyvk/src build/easyvk.o  runner.cpp $(VULKAN_LIB) -lvulkan-1 -static -o build/runner.run

android: build $(SPVS)
	ndk-build APP_BUILD_SCRIPT=./Android.mk  NDK_PROJECT_PATH=. NDK_APPLICATION_MK=./Application.mk NDK_LIBS_OUT=./build/android/libs NDK_OUT=./build/android/obj

%.spv: %.cl
	clspv $(CLSPVFLAGS) $< -o $@

%.cinit: %.cl
	clspv $(CLSPVFLAGS) --output-format=c  $< -o $@

clean:
	rm -rf build
