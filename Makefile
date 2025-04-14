# Makefile for Node-Based Image Processor (Ubuntu 24.04)

# Compiler and flags
CXX = g++
CXXFLAGS = -std=c++17 -Wall -Wextra -O2 -g

# Include paths for Ubuntu 24.04 - Modified to include imgui and imnodes directories
INCLUDES = -I. -I./imgui -I./imgui/backends -I./imnodes -I/usr/include/opencv4

# Get OpenCV libraries using pkg-config
OPENCV_LIBS = $(shell pkg-config --libs opencv4)
OPENCV_CFLAGS = $(shell pkg-config --cflags opencv4)

# Libraries for Ubuntu 24.04
LIBS = -lGL -lglfw -ldl -pthread $(OPENCV_LIBS)

# Source files
SOURCES = main.cpp \
          imgui/imgui.cpp \
          imgui/imgui_demo.cpp \
          imgui/imgui_draw.cpp \
          imgui/imgui_tables.cpp \
          imgui/imgui_widgets.cpp \
          imgui/backends/imgui_impl_glfw.cpp \
          imgui/backends/imgui_impl_opengl3.cpp \
          imnodes/imnodes.cpp

# Object files
OBJECTS = $(SOURCES:.cpp=.o)

# Executable name
TARGET = node_image_processor

# Build rules
all: $(TARGET)

$(TARGET): $(OBJECTS)
	$(CXX) $(CXXFLAGS) -o $@ $^ $(LIBS)

%.o: %.cpp
	$(CXX) $(CXXFLAGS) $(INCLUDES) $(OPENCV_CFLAGS) -c -o $@ $<

# Install dependencies
deps:
	sudo apt-get update
	sudo apt-get install -y build-essential libopencv-dev libglfw3-dev libgl1-mesa-dev

# Clean rule
clean:
	rm -f $(OBJECTS) $(TARGET)

.PHONY: all clean deps