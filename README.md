# node-based-image-processor

## Introduction
This is a beginner's attempt at making a node based image editor.
I am making this project as a part of the internship seletion process by Mixar.

## Overview

This C++ application provides a node-based interface for image manipulation. Users can load images, process them through a series of connected nodes, and save the resulting output. The interface is designed to function similarly to node-based editors like Substance Designer, where operations are represented visually and can be connected in various configurations. [cite: 1, 2, 3, 4]

## Features Implemented

* A graphical user interface with a canvas for creating and connecting nodes. [cite: 4]
* Buttons and sliders on the nodes to edit the node parameters. [cite: 4]
* Basic file operations: open image, save result. [cite: 4]
* Node system with:
    * Input nodes for loading images. [cite: 4]
    * Processing nodes for manipulating images. [cite: 4]
    * Output nodes for saving results. [cite: 4]
* Connection system to link nodes together and define the processing pipeline. [cite: 4]
* Real-time preview of results when parameters are changed. [cite: 4]
* Proper error handling for invalid connections and operations. [cite: 4]
* 5 different processing nodes:
    * **Basic Nodes:**
        * Image Input Node: Loads images from the file system, displays image metadata, and supports common image formats (JPG, PNG, BMP). [cite: 5]
        * Output Node: Saves processed images to disk, allows selection of output format and quality settings, and displays a preview of the final image. [cite: 5]
        * Brightness/Contrast Node: Adjusts image brightness and contrast with sliders and provides reset buttons. [cite: 5, 6]
        * Color Channel Splitter: Splits RGB/RGBA images into separate channel outputs with an option to output grayscale representations. [cite: 6]
    * **Intermediate Nodes:**
        * Blur Node: Implements Gaussian blur with configurable radius and options for uniform or directional blur. [cite: 6]
        <!-- * Threshold Node: Converts images to binary images based on a threshold value, with options for different thresholding methods and a histogram display. [cite: 6] -->
        * Edge Detection Node: Implements Sobel and Canny edge detection algorithms with configurable parameters and an option to overlay edges on the original image. [cite: 6, 7]
        * Blend Node: Combines two images using different blend modes (normal, multiply, screen, overlay, difference) and includes an opacity/mix slider. [cite: 7]
        -->
* Graph-based execution system that:
    * Detects circular dependencies. [cite: 8]
    * Processes nodes in the correct order. [cite: 8]
    * Caches results to avoid redundant processing. [cite: 8]

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
    * To compile this code, just use the Makefile provided.
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
