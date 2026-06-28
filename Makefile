# If RACK_DIR is not defined when calling the Makefile, default to two directories above
RACK_DIR ?= ../..

FLAGS +=
CFLAGS +=
CXXFLAGS +=

LDFLAGS +=

# Add .cpp files to the build
SOURCES += $(wildcard src/*.cpp)

# Add files to the ZIP package when running `make dist`
DISTRIBUTABLES += res
DISTRIBUTABLES += $(wildcard LICENSE*)
DISTRIBUTABLES += $(wildcard presets)

include $(RACK_DIR)/plugin.mk