# node-based-image-processor

## Introduction
This is a beginner's attempt at making a node-based image editor.
I am making this project part of Mixar's internship selection process.

## Overview

This C++ application provides a node-based interface for image manipulation. Users can load images, process them through a series of connected nodes, and save the resulting output. The interface is designed to function similarly to node-based editors like Substance Designer, where operations are represented visually and can be connected in various configurations.

## Features Implemented

* A graphical user interface with a canvas for creating and connecting nodes.
* Buttons and sliders on the nodes to edit the node parameters.
* Basic file operations: open image, save result.
* Node system with:
    * Input nodes for loading images.
    * Processing nodes for manipulating images.
    * Output nodes for saving results.
* Connection system to link nodes together and define the processing pipeline.
* Real-time preview of results when parameters are changed.
* Proper error handling for invalid connections and operations.
* 5 different processing nodes:
    * **Basic Nodes:**
        * Image Input Node: Loads images from the file system, displays image metadata, and supports common image formats (JPG, PNG, BMP).
        * Output Node: Saves processed images to disk, allows selection of output format and quality settings and displays a final image preview.
        * Brightness/Contrast Node: Adjusts image brightness and contrast with sliders and provides reset buttons.
        * Color Channel Splitter: Splits RGB/RGBA images into separate channel outputs with an option to output grayscale representations.
    * **Intermediate Nodes:**
        * Blur Node: Implements Gaussian blur with configurable radius and uniform or directional blur options.
        <!-- * Threshold Node: Converts images to binary images based on a threshold value, with options for different thresholding methods and a histogram display.
        * Edge Detection Node: Implements Sobel and Canny edge detection algorithms with configurable parameters and an option to overlay edges on the original image.
        * Blend Node: Combines two images using different blend modes (normal, multiply, screen, overlay, difference) and includes an opacity/mix slider.
        -->
* Graph-based execution system that:
    * Detects circular dependencies. 
    * Processes nodes in the correct order. 
    * Caches results to avoid redundant processing. 

## Build Instructions

1.  **Prerequisites:**
    * C++ compiler (e.g., g++, Visual Studio)
    * OpenCV library (or similar image processing library)
2.  **Clone the repository:**
    ```bash
    git clone https://github.com/Wingbound/node-based-image-processor.git
    ```
3.  **Build the application:**
    * Instructions primarily for Linux.
    * You must preinstall the openCV libraries to use this program.
    * To compile this code, use the Makefile provided.
    * Run the following commands to install the dependencies and compile the code:
    ```bash
    make deps
    make clean
    make
    ```
4.  **Run the application:**
    ```bash
    ./node_based_image_processor
    ```
5. **Documentation**
   * The third-party libraries used are imgui and imnodes. They are used primarily because of the ease of using these repositories and because of my inability to properly use the QT repositories, which are popularly used for these kinds of applications.
