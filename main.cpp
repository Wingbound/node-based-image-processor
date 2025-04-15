#include <iostream>
#include <string>
#include <vector>
#include <map>
#include <memory>
#include <algorithm>
#include <functional>
#include <cmath>

#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl3.h"
#include "imnodes.h"
#include <opencv2/opencv.hpp>
#include <GLFW/glfw3.h>

// Forward declarations
class Node;
class Connection;
class NodeEditor;
class ImageProcessor;

// Connection between nodes
struct Connection {
    int inputNodeId;
    int outputNodeId;
    int inputPinId;
    int outputPinId;

    Connection(int in_node, int out_node, int in_pin, int out_pin)
        : inputNodeId(in_node), outputNodeId(out_node), inputPinId(in_pin), outputPinId(out_pin) {}

    bool operator==(const Connection& other) const {
        return inputNodeId == other.inputNodeId && outputNodeId == other.outputNodeId &&
               inputPinId == other.inputPinId && outputPinId == other.outputPinId;
    }
};

// Base Node class
class Node {
protected:
    int id;
    std::string name;
    std::vector<int> inputPins;
    std::vector<int> outputPins;
    int nextPinId = 1;

    // Cache for processed image
    cv::Mat processedImage;
    bool needsProcessing = true;

public:
    Node(int id, const std::string& name) : id(id), name(name) {}
    virtual ~Node() {}

    virtual void process(const std::map<int, std::shared_ptr<Node>>& nodes, 
                         const std::vector<Connection>& connections) = 0;
    
    virtual void drawNodeContent() = 0;
    
    int getId() const { return id; }
    std::string getName() const { return name; }
    const std::vector<int>& getInputPins() const { return inputPins; }
    const std::vector<int>& getOutputPins() const { return outputPins; }
    
    int addInputPin() {
        int pinId = id * 1000 + nextPinId++;
        inputPins.push_back(pinId);
        return pinId;
    }
    
    int addOutputPin() {
        int pinId = id * 1000 + nextPinId++;
        outputPins.push_back(pinId);
        return pinId;
    }

    cv::Mat getOutputImage(int pinIndex = 0) const {
        (void)pinIndex; // Avoid unused parameter warning
        return processedImage;
    }

    void setNeedsProcessing() {
        needsProcessing = true;
    }

    bool getNeedsProcessing() const {
        return needsProcessing;
    }

    void markProcessed() {
        needsProcessing = false;
    }

    // Get connected input node
    std::shared_ptr<Node> getConnectedInputNode(int inputPinId, 
                                                const std::map<int, std::shared_ptr<Node>>& nodes, 
                                                const std::vector<Connection>& connections,
                                                int& outputPinIndex) {
        for (const auto& conn : connections) {
            if (conn.inputNodeId == id && conn.inputPinId == inputPinId) {
                auto it = nodes.find(conn.outputNodeId);
                if (it != nodes.end()) {
                    // Find the output pin index
                    auto& outputPins = it->second->getOutputPins();
                    for (size_t i = 0; i < outputPins.size(); i++) {
                        if (outputPins[i] == conn.outputPinId) {
                            outputPinIndex = i;
                            return it->second;
                        }
                    }
                }
            }
        }
        return nullptr;
    }
};

// Input Node for loading images
class ImageInputNode : public Node {
private:
    std::string filePath;
    cv::Mat image;
    bool imageLoaded = false;
    GLuint textureID = 0;

    void loadImageToTexture() {
        if (!imageLoaded) return;
        
        if (textureID) {
            glDeleteTextures(1, &textureID);
        }

        // Create OpenGL texture
        glGenTextures(1, &textureID);
        glBindTexture(GL_TEXTURE_2D, textureID);
        
        // Setup filtering parameters for display
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

        // Upload pixels into texture
        cv::Mat rgbImage;
        cv::cvtColor(image, rgbImage, cv::COLOR_BGR2RGB);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, rgbImage.cols, rgbImage.rows, 0, GL_RGB, GL_UNSIGNED_BYTE, rgbImage.data);
    }

public:
    ImageInputNode(int id) : Node(id, "Image Input") {
        addOutputPin(); // Single output pin
    }

    ~ImageInputNode() {
        if (textureID) {
            glDeleteTextures(1, &textureID);
        }
    }

    void loadImage(const std::string& path) {
        filePath = path;
        image = cv::imread(path);
        imageLoaded = !image.empty();
        if (imageLoaded) {
            processedImage = image.clone();
            loadImageToTexture();
            setNeedsProcessing();
        }
    }

    void process(const std::map<int, std::shared_ptr<Node>>& nodes, 
                 const std::vector<Connection>& connections) override {
        (void)nodes; // Avoid unused parameter warning
        (void)connections; // Avoid unused parameter warning
        
        if (getNeedsProcessing() && imageLoaded) {
            processedImage = image.clone();
            markProcessed();
        }
    }

    void drawNodeContent() override {
        ImGui::Text("Image Path: %s", filePath.c_str());
        
        if (ImGui::Button("Load Image")) {
            // In a real app, you'd use a file dialog here
            filePath = "input.png"; // Placeholder
            loadImage(filePath);
        }

        if (imageLoaded) {
            ImGui::Text("Size: %dx%d", image.cols, image.rows);
            ImGui::Text("Channels: %d", image.channels());
            
            // Display image preview - Fixed casting for ImTextureID
            ImGui::Image(reinterpret_cast<ImTextureID>(static_cast<unsigned long long int>(textureID)), ImVec2(100, 100));
        }

        // Draw output pin
        ImNodes::BeginOutputAttribute(outputPins[0]);
        ImGui::Text("Output");
        ImNodes::EndOutputAttribute();
    }
};

// Brightness/Contrast Node
class BrightnessContrastNode : public Node {
private:
    float brightness = 0.0f;
    float contrast = 1.0f;

public:
    BrightnessContrastNode(int id) : Node(id, "Brightness/Contrast") {
        addInputPin();
        addOutputPin();
    }

    void process(const std::map<int, std::shared_ptr<Node>>& nodes, 
                 const std::vector<Connection>& connections) override {
        if (!getNeedsProcessing()) return;

        int outputPinIndex = 0;
        auto inputNode = getConnectedInputNode(inputPins[0], nodes, connections, outputPinIndex);
        
        if (inputNode) {
            // Make sure the input node is processed
            if (inputNode->getNeedsProcessing()) {
                inputNode->process(nodes, connections);
            }
            
            cv::Mat inputImage = inputNode->getOutputImage(outputPinIndex);
            if (!inputImage.empty()) {
                inputImage.convertTo(processedImage, -1, contrast, brightness);
                markProcessed();
            }
        }
    }

    void drawNodeContent() override {
        // Input pin
        ImNodes::BeginInputAttribute(inputPins[0]);
        ImGui::Text("Input");
        ImNodes::EndInputAttribute();

        // Node controls
        bool changed = false;
        changed |= ImGui::SliderFloat("Brightness", &brightness, -100.0f, 100.0f);
        changed |= ImGui::SliderFloat("Contrast", &contrast, 0.0f, 3.0f);
        
        if (ImGui::Button("Reset")) {
            brightness = 0.0f;
            contrast = 1.0f;
            changed = true;
        }

        if (changed) {
            setNeedsProcessing();
        }

        // Output pin
        ImNodes::BeginOutputAttribute(outputPins[0]);
        ImGui::Text("Output");
        ImNodes::EndOutputAttribute();
    }
};

// Output Node for saving images
class OutputNode : public Node {
private:
    std::string outputPath = "output.jpg";
    int quality = 95;
    GLuint textureID = 0;
    bool hasPreview = false;

    void updatePreviewTexture(const cv::Mat& img) {
        if (img.empty()) {
            hasPreview = false;
            return;
        }

        if (textureID) {
            glDeleteTextures(1, &textureID);
        }

        // Create OpenGL texture
        glGenTextures(1, &textureID);
        glBindTexture(GL_TEXTURE_2D, textureID);
        
        // Setup filtering parameters for display
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

        // Upload pixels into texture
        cv::Mat rgbImage;
        cv::cvtColor(img, rgbImage, cv::COLOR_BGR2RGB);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, rgbImage.cols, rgbImage.rows, 0, GL_RGB, GL_UNSIGNED_BYTE, rgbImage.data);
        
        hasPreview = true;
    }

public:

    GLuint getPreviewTextureID() const {
        return textureID;
    }
    OutputNode(int id) : Node(id, "Output") {
        addInputPin();
    }

    ~OutputNode() {
        if (textureID) {
            glDeleteTextures(1, &textureID);
        }
    }

    void process(const std::map<int, std::shared_ptr<Node>>& nodes, 
                 const std::vector<Connection>& connections) override {
        if (!getNeedsProcessing()) return;

        int outputPinIndex = 0;
        auto inputNode = getConnectedInputNode(inputPins[0], nodes, connections, outputPinIndex);
        
        if (inputNode) {
            // Make sure the input node is processed
            if (inputNode->getNeedsProcessing()) {
                inputNode->process(nodes, connections);
            }
            
            cv::Mat inputImage = inputNode->getOutputImage(outputPinIndex);
            if (!inputImage.empty()) {
                processedImage = inputImage.clone();
                updatePreviewTexture(processedImage);
                markProcessed();
            }
        }
    }

    void saveOutput() {
        if (!processedImage.empty()) {
            std::vector<int> params;
            if (outputPath.find(".jpg") != std::string::npos || 
                outputPath.find(".jpeg") != std::string::npos) {
                params.push_back(cv::IMWRITE_JPEG_QUALITY);
                params.push_back(quality);
            }
            cv::imwrite(outputPath, processedImage, params);
        }
    }

    void drawNodeContent() override {
        // Input pin
        ImNodes::BeginInputAttribute(inputPins[0]);
        ImGui::Text("Input");
        ImNodes::EndInputAttribute();

        // Node controls
        char buffer[256];
        strcpy(buffer, outputPath.c_str());
        if (ImGui::InputText("Output Path", buffer, sizeof(buffer))) {
            outputPath = buffer;
        }

        ImGui::SliderInt("Quality", &quality, 1, 100);
        
        if (ImGui::Button("Save Image")) {
            saveOutput();
        }
        
        // Preview
        if (hasPreview) {
            ImGui::Text("Preview:");
            ImGui::Image(reinterpret_cast<ImTextureID>(static_cast<unsigned long long int>(textureID)), ImVec2(150, 150));
        }
    }
};

// Color Channel Splitter Node
class ColorChannelSplitterNode : public Node {
    private:
        bool outputGrayscale = false;
        GLuint channelTextures[4] = {0};  // R, G, B, A
        std::vector<cv::Mat> splitChannels;
        
        // Change in updateTextures method
void updateTextures(const std::vector<cv::Mat>& channels) {
    // Clean up previous textures
    for (int i = 0; i < 4; i++) {
        if (channelTextures[i]) {
            glDeleteTextures(1, &channelTextures[i]);
            channelTextures[i] = 0;
        }
    }
    
    // Create textures for each channel
    for (size_t i = 0; i < channels.size(); i++) {
        if (!channels[i].empty()) {
            glGenTextures(1, &channelTextures[i]);
            glBindTexture(GL_TEXTURE_2D, channelTextures[i]);
            
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
            
            // For grayscale output, use the channel directly
            // For color output, create a colored version based on the channel
            cv::Mat displayChannel;
            if (outputGrayscale) {
                displayChannel = channels[i].clone();
                glTexImage2D(GL_TEXTURE_2D, 0, GL_LUMINANCE, displayChannel.cols, displayChannel.rows, 
                           0, GL_LUMINANCE, GL_UNSIGNED_BYTE, displayChannel.data);
            } else {
                // Create colored version for preview
                cv::Mat colored;
                if (i == 0) {  // Red
                    cv::Mat zeros = cv::Mat::zeros(channels[i].size(), CV_8UC1);
                    std::vector<cv::Mat> components = {zeros, zeros, channels[i]};
                    cv::merge(components, colored);
                } else if (i == 1) {  // Green
                    cv::Mat zeros = cv::Mat::zeros(channels[i].size(), CV_8UC1);
                    std::vector<cv::Mat> components = {zeros, channels[i], zeros};
                    cv::merge(components, colored);
                } else if (i == 2) {  // Blue
                    cv::Mat zeros = cv::Mat::zeros(channels[i].size(), CV_8UC1);
                    std::vector<cv::Mat> components = {channels[i], zeros, zeros};
                    cv::merge(components, colored);
                } else {  // Alpha - keep as grayscale
                    cv::cvtColor(channels[i], colored, cv::COLOR_GRAY2BGR);
                }
                
                glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, colored.cols, colored.rows, 
                           0, GL_BGR, GL_UNSIGNED_BYTE, colored.data);
            }
        }
    }
}
    
    public:
        ColorChannelSplitterNode(int id) : Node(id, "Color Channel Splitter") {
            addInputPin();
            // Add 4 output pins for R, G, B, A channels
            for (int i = 0; i < 4; i++) {
                addOutputPin();
            }
            for (int i = 0; i < 4; i++) {
                splitChannels.push_back(cv::Mat());
            }
        }
        
        ~ColorChannelSplitterNode() {
            for (int i = 0; i < 4; i++) {
                if (channelTextures[i]) {
                    glDeleteTextures(1, &channelTextures[i]);
                }
            }
        }
    
        void process(const std::map<int, std::shared_ptr<Node>>& nodes, 
                     const std::vector<Connection>& connections) override {
            if (!getNeedsProcessing()) return;
    
            int outputPinIndex = 0;
            auto inputNode = getConnectedInputNode(inputPins[0], nodes, connections, outputPinIndex);
            
            if (inputNode) {
                // Make sure the input node is processed
                if (inputNode->getNeedsProcessing()) {
                    inputNode->process(nodes, connections);
                }
                
                cv::Mat inputImage = inputNode->getOutputImage(outputPinIndex);
                if (!inputImage.empty()) {
                    // Split the channels
                    std::vector<cv::Mat> channels;
                    cv::split(inputImage, channels);

                    // Store the original channels so we can access them later
                    splitChannels.clear();
                    for (size_t i = 0; i < channels.size(); i++) {
                        splitChannels.push_back(channels[i].clone());
                    }

                    // Ensure we have 4 channels (R, G, B, A)
                    while (splitChannels.size() < 4) {
                        cv::Mat alpha = cv::Mat::ones(inputImage.size(), CV_8UC1) * 255;
                        splitChannels.push_back(alpha);
                    }
                    
                    // Ensure we have 4 channels (R, G, B, A), even if input doesn't have alpha
                    while (channels.size() < 4) {
                        // Create an empty alpha channel if needed
                        cv::Mat alpha = cv::Mat::ones(inputImage.size(), CV_8UC1) * 255;
                        channels.push_back(alpha);
                    }

                    if (outputGrayscale) {
                        // For grayscale output, keep each channel as is
                        processedImage = splitChannels[0]; // Store R in processedImage for main output
                    } else {
                        // For colored visualization, create the red version for main output
                        cv::Mat redColored = cv::Mat::zeros(inputImage.size(), CV_8UC3);
                        cv::Mat blueZero = cv::Mat::zeros(inputImage.size(), CV_8UC1);
                        cv::Mat greenZero = cv::Mat::zeros(inputImage.size(), CV_8UC1);
                        std::vector<cv::Mat> redComponents = {blueZero, greenZero, splitChannels[0]};
                        cv::merge(redComponents, redColored);
                        processedImage = redColored;
                    }

                    updateTextures(splitChannels);
                    
                    markProcessed();
                }
            }
        }
    
        cv::Mat getOutputImage(int pinIndex) const {
            // Output pin 0 = R, 1 = G, 2 = B, 3 = A
            if (pinIndex >= 0 && pinIndex < outputPins.size() && pinIndex < splitChannels.size()) {
                // Return the corresponding channel from our stored split channels
                return splitChannels[pinIndex];
            }
            
            // If the pin index doesn't match or something went wrong
            return cv::Mat();
        }
    
        void drawNodeContent() override {
            // Input pin
            ImNodes::BeginInputAttribute(inputPins[0]);
            ImGui::Text("Input");
            ImNodes::EndInputAttribute();
    
            // Node controls
            bool changed = ImGui::Checkbox("Output as Grayscale", &outputGrayscale);
            
            if (changed) {
                setNeedsProcessing();
            }
            
            // Display channel previews if we have processed image
            if (!processedImage.empty()) {
                ImGui::Text("Channel Previews:");
                
                const char* channelNames[4] = {"R", "G", "B", "A"};
                float previewSize = 80.0f;
                
                for (int i = 0; i < 4; i++) {
                    if (channelTextures[i]) {
                        ImGui::PushID(i);
                        ImGui::Text("%s", channelNames[i]);
                        ImGui::Image(reinterpret_cast<ImTextureID>(static_cast<unsigned long long int>(channelTextures[i])), 
                                    ImVec2(previewSize, previewSize));
                        ImGui::PopID();
                        
                        if (i < 3) ImGui::SameLine();
                    }
                }
            }
    
            // Output pins
            const char* pinLabels[4] = {"R Out", "G Out", "B Out", "A Out"};
            for (int i = 0; i < 4; i++) {
                ImNodes::BeginOutputAttribute(outputPins[i]);
                ImGui::Text("%s", pinLabels[i]);
                ImNodes::EndOutputAttribute();
            }
        }
    };

// Blur Node
class BlurNode : public Node {
    private:
        int blurRadius = 5;
        bool directionalBlur = false;
        float angleInDegrees = 0.0f;
        bool showKernel = false;
        std::vector<float> kernelValues;
        
        void updateKernelPreview() {
            kernelValues.clear();
            if (directionalBlur) {
                // For directional blur, create a motion blur kernel
                float angleInRadians = angleInDegrees * M_PI / 180.0f;
                float dx = cos(angleInRadians);
                float dy = sin(angleInRadians);
                
                int size = 2 * blurRadius + 1;
                cv::Mat kernel = cv::Mat::zeros(size, size, CV_32F);
                cv::Point center(blurRadius, blurRadius);
                
                // Create a directional kernel
                for (int i = -blurRadius; i <= blurRadius; ++i) {
                    int x = center.x + static_cast<int>(dx * i);
                    int y = center.y + static_cast<int>(dy * i);
                    
                    // Ensure we're within bounds
                    if (x >= 0 && x < size && y >= 0 && y < size) {
                        kernel.at<float>(y, x) = 1.0f;
                    }
                }
                
                // Normalize the kernel
                kernel = kernel / cv::sum(kernel)[0];
                
                // Copy to kernelValues for display
                for (int y = 0; y < size; ++y) {
                    for (int x = 0; x < size; ++x) {
                        kernelValues.push_back(kernel.at<float>(y, x));
                    }
                }
            } else {
                // For uniform Gaussian blur, we don't actually need to create the kernel
                // as OpenCV's GaussianBlur will do it for us, but we'll create it for visualization
                int size = 2 * blurRadius + 1;
                cv::Mat kernel = cv::Mat::zeros(size, size, CV_32F);
                cv::getGaussianKernel(size, blurRadius, CV_32F).copyTo(kernel);
                
                // Make it 2D
                kernel = kernel * kernel.t();
                
                // Copy to kernelValues for display
                for (int y = 0; y < size; ++y) {
                    for (int x = 0; x < size; ++x) {
                        kernelValues.push_back(kernel.at<float>(y, x));
                    }
                }
            }
        }
    
    public:
        BlurNode(int id) : Node(id, "Blur") {
            addInputPin();
            addOutputPin();
            updateKernelPreview();
        }
    
        void process(const std::map<int, std::shared_ptr<Node>>& nodes, 
                     const std::vector<Connection>& connections) override {
            if (!getNeedsProcessing()) return;
    
            int outputPinIndex = 0;
            auto inputNode = getConnectedInputNode(inputPins[0], nodes, connections, outputPinIndex);
            
            if (inputNode) {
                // Make sure the input node is processed
                if (inputNode->getNeedsProcessing()) {
                    inputNode->process(nodes, connections);
                }
                
                cv::Mat inputImage = inputNode->getOutputImage(outputPinIndex);
                if (!inputImage.empty()) {
                    if (directionalBlur) {
                        // Create a motion blur kernel
                        float angleInRadians = angleInDegrees * M_PI / 180.0f;
                        cv::Size ksize(0, 0);
                        cv::Point2f dir(cos(angleInRadians), sin(angleInRadians));
                        cv::Mat kernel = cv::getGaussianKernel(2 * blurRadius + 1, blurRadius, CV_32F);
                        cv::Mat kernel2D = kernel * kernel.t();
                        
                        // Apply the motion blur
                        cv::filter2D(inputImage, processedImage, -1, kernel2D);
                    } else {
                        // Apply standard Gaussian blur
                        cv::Size ksize(2 * blurRadius + 1, 2 * blurRadius + 1);
                        cv::GaussianBlur(inputImage, processedImage, ksize, 0);
                    }
                    markProcessed();
                }
            }
        }
    
        void drawNodeContent() override {
            // Input pin
            ImNodes::BeginInputAttribute(inputPins[0]);
            ImGui::Text("Input");
            ImNodes::EndInputAttribute();
    
            // Node controls
            bool changed = false;
            changed |= ImGui::SliderInt("Radius", &blurRadius, 1, 20);
            changed |= ImGui::Checkbox("Directional Blur", &directionalBlur);
            
            if (directionalBlur) {
                changed |= ImGui::SliderFloat("Angle", &angleInDegrees, 0.0f, 360.0f);
            }
            
            ImGui::Checkbox("Show Kernel", &showKernel);
            
            if (changed) {
                updateKernelPreview();
                setNeedsProcessing();
            }
            
            // Display kernel preview
            if (showKernel && !kernelValues.empty()) {
                ImGui::Text("Kernel Preview:");
                int size = 2 * blurRadius + 1;
                float cellSize = std::min(100.0f / size, 20.0f);
                
                ImGui::BeginChild("KernelPreview", ImVec2(size * cellSize + 20, size * cellSize + 20), true);
                ImDrawList* draw_list = ImGui::GetWindowDrawList();
                
                ImVec2 canvas_p0 = ImGui::GetCursorScreenPos();
                ImVec2 canvas_sz = ImVec2(size * cellSize, size * cellSize);
                ImVec2 canvas_p1 = ImVec2(canvas_p0.x + canvas_sz.x, canvas_p0.y + canvas_sz.y);
                
                // Find max value for normalization
                float maxVal = 0.0f;
                for (float val : kernelValues) {
                    maxVal = std::max(maxVal, val);
                }
                
                // Draw cells
                for (int y = 0; y < size; ++y) {
                    for (int x = 0; x < size; ++x) {
                        float val = kernelValues[y * size + x] / maxVal;
                        ImVec2 cell_p0 = ImVec2(canvas_p0.x + x * cellSize, canvas_p0.y + y * cellSize);
                        ImVec2 cell_p1 = ImVec2(cell_p0.x + cellSize, cell_p0.y + cellSize);
                        
                        ImU32 color = ImGui::GetColorU32(ImVec4(val, val, val, 1.0f));
                        draw_list->AddRectFilled(cell_p0, cell_p1, color);
                        draw_list->AddRect(cell_p0, cell_p1, IM_COL32(50, 50, 50, 255));
                    }
                }
                
                ImGui::EndChild();
            }
    
            // Output pin
            ImNodes::BeginOutputAttribute(outputPins[0]);
            ImGui::Text("Output");
            ImNodes::EndOutputAttribute();
        }
    };

// Threshold Node
class ThresholdNode : public Node {
    private:
        int thresholdValue = 127;
        int thresholdType = 0; // 0: Binary, 1: Binary Inverted, 2: Truncate, 3: ToZero, 4: ToZero Inverted
        int adaptiveMethod = 0; // 0: None (global), 1: Mean, 2: Gaussian
        int blockSize = 11;
        int constant = 2;
        bool useOtsu = false;
        
        // Histogram data
        std::vector<int> histogram;
        GLuint histogramTexture = 0;
        
        void updateHistogram(const cv::Mat& image) {
            // Clear previous histogram
            histogram.clear();
            histogram.resize(256, 0);
            
            // Calculate histogram
            if (image.empty() || image.channels() > 1) return;
            
            for (int y = 0; y < image.rows; y++) {
                for (int x = 0; x < image.cols; x++) {
                    uchar pixelValue = image.at<uchar>(y, x);
                    histogram[pixelValue]++;
                }
            }
            
            // Create histogram visualization texture
            if (histogramTexture) {
                glDeleteTextures(1, &histogramTexture);
            }
            
            // Find maximum count for normalization
            int maxCount = *std::max_element(histogram.begin(), histogram.end());
            if (maxCount == 0) return;
            
            // Create histogram image
            cv::Mat histImage = cv::Mat::zeros(200, 256, CV_8UC3);
            for (int i = 0; i < 256; i++) {
                int height = cvRound(histogram[i] * 180.0 / maxCount);
                cv::line(histImage, 
                         cv::Point(i, histImage.rows), 
                         cv::Point(i, histImage.rows - height), 
                         cv::Scalar(200, 200, 200));
                
                // Mark the current threshold value
                if (i == thresholdValue) {
                    cv::line(histImage, 
                             cv::Point(i, histImage.rows), 
                             cv::Point(i, 0), 
                             cv::Scalar(0, 0, 255));
                }
            }
            
            // Convert to texture
            glGenTextures(1, &histogramTexture);
            glBindTexture(GL_TEXTURE_2D, histogramTexture);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
            cv::Mat rgbHistImage;
            cv::cvtColor(histImage, rgbHistImage, cv::COLOR_BGR2RGB);
            glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, rgbHistImage.cols, rgbHistImage.rows, 
                        0, GL_RGB, GL_UNSIGNED_BYTE, rgbHistImage.data);
        }
    
    public:
        ThresholdNode(int id) : Node(id, "Threshold") {
            addInputPin();
            addOutputPin();
        }
        
        ~ThresholdNode() {
            if (histogramTexture) {
                glDeleteTextures(1, &histogramTexture);
            }
        }
    
        void process(const std::map<int, std::shared_ptr<Node>>& nodes, 
                     const std::vector<Connection>& connections) override {
            if (!getNeedsProcessing()) return;
    
            int outputPinIndex = 0;
            auto inputNode = getConnectedInputNode(inputPins[0], nodes, connections, outputPinIndex);
            
            if (inputNode) {
                // Make sure the input node is processed
                if (inputNode->getNeedsProcessing()) {
                    inputNode->process(nodes, connections);
                }
                
                cv::Mat inputImage = inputNode->getOutputImage(outputPinIndex);
                if (!inputImage.empty()) {
                    // Convert to grayscale if necessary
                    cv::Mat grayImage;
                    if (inputImage.channels() > 1) {
                        cv::cvtColor(inputImage, grayImage, cv::COLOR_BGR2GRAY);
                    } else {
                        grayImage = inputImage.clone();
                    }
                    
                    // Update histogram
                    updateHistogram(grayImage);
                    
                    // Perform thresholding based on selected method
                    if (adaptiveMethod == 0) { // Global thresholding
                        int type = thresholdType;
                        if (useOtsu) {
                            type += cv::THRESH_OTSU;
                        }
                        cv::threshold(grayImage, processedImage, thresholdValue, 255, type);
                    } else { // Adaptive thresholding
                        int adaptiveType = (adaptiveMethod == 1) ? 
                            cv::ADAPTIVE_THRESH_MEAN_C : cv::ADAPTIVE_THRESH_GAUSSIAN_C;
                        
                        // Make sure blockSize is odd
                        int blockSizeOdd = (blockSize % 2 == 0) ? blockSize + 1 : blockSize;
                        
                        cv::adaptiveThreshold(grayImage, processedImage, 255, 
                                             adaptiveType, thresholdType, 
                                             blockSizeOdd, constant);
                    }
                    
                    markProcessed();
                }
            }
        }
    
        void drawNodeContent() override {
            // Input pin
            ImNodes::BeginInputAttribute(inputPins[0]);
            ImGui::Text("Input");
            ImNodes::EndInputAttribute();
    
            // Node controls
            bool changed = false;
            
            // Threshold method selection
            const char* thresholdMethods[] = { "Global", "Adaptive" };
            changed |= ImGui::Combo("Method", &adaptiveMethod, thresholdMethods, IM_ARRAYSIZE(thresholdMethods));
            
            if (adaptiveMethod == 0) { // Global threshold
                changed |= ImGui::SliderInt("Threshold", &thresholdValue, 0, 255);
                
                const char* thresholdTypes[] = { "Binary", "Binary Inv", "Truncate", "To Zero", "To Zero Inv" };
                changed |= ImGui::Combo("Type", &thresholdType, thresholdTypes, IM_ARRAYSIZE(thresholdTypes));
                
                changed |= ImGui::Checkbox("Use Otsu", &useOtsu);
                if (useOtsu) {
                    ImGui::TextDisabled("(Threshold slider ignored when Otsu is enabled)");
                }
            } else { // Adaptive threshold
                const char* adaptiveMethods[] = { "Mean", "Gaussian" };
                changed |= ImGui::Combo("Adaptive Method", &adaptiveMethod, adaptiveMethods, IM_ARRAYSIZE(adaptiveMethods));
                
                const char* adaptiveTypes[] = { "Binary", "Binary Inv" };
                changed |= ImGui::Combo("Type", &thresholdType, adaptiveTypes, 2);
                
                changed |= ImGui::SliderInt("Block Size", &blockSize, 3, 51, "%d");
                ImGui::Text("(Block size must be odd)");
                
                changed |= ImGui::SliderInt("Constant", &constant, -10, 10);
            }
            
            if (changed) {
                setNeedsProcessing();
            }
            
            // Display histogram
            if (histogramTexture) {
                ImGui::Text("Histogram:");
                ImGui::Image(reinterpret_cast<ImTextureID>(static_cast<unsigned long long int>(histogramTexture)), 
                             ImVec2(256, 100));
            }
    
            // Output pin
            ImNodes::BeginOutputAttribute(outputPins[0]);
            ImGui::Text("Output");
            ImNodes::EndOutputAttribute();
        }
    };

// Edge Detection Node
class EdgeDetectionNode : public Node {
    private:
        int algorithm = 0; // 0: Sobel, 1: Canny
        int sobelKSize = 3;
        int sobelDx = 1;
        int sobelDy = 1;
        int cannyThreshold1 = 50;
        int cannyThreshold2 = 150;
        int cannyApertureSize = 3;
        bool overlay = false;
        float overlayWeight = 0.7;
        
        GLuint previewTexture = 0;
        cv::Mat edgeImage;
        
        void updatePreviewTexture() {
            if (edgeImage.empty()) return;
            
            if (previewTexture) {
                glDeleteTextures(1, &previewTexture);
            }
            
            // Create OpenGL texture
            glGenTextures(1, &previewTexture);
            glBindTexture(GL_TEXTURE_2D, previewTexture);
            
            // Setup filtering parameters
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
            
            // Upload pixels into texture
            cv::Mat rgbEdge;
            cv::cvtColor(edgeImage, rgbEdge, cv::COLOR_GRAY2RGB);
            glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, rgbEdge.cols, rgbEdge.rows, 
                        0, GL_RGB, GL_UNSIGNED_BYTE, rgbEdge.data);
        }
    
    public:
        EdgeDetectionNode(int id) : Node(id, "Edge Detection") {
            addInputPin();
            addOutputPin();
        }
        
        ~EdgeDetectionNode() {
            if (previewTexture) {
                glDeleteTextures(1, &previewTexture);
            }
        }
    
        void process(const std::map<int, std::shared_ptr<Node>>& nodes, 
                     const std::vector<Connection>& connections) override {
            if (!getNeedsProcessing()) return;
    
            int outputPinIndex = 0;
            auto inputNode = getConnectedInputNode(inputPins[0], nodes, connections, outputPinIndex);
            
            if (inputNode) {
                // Make sure the input node is processed
                if (inputNode->getNeedsProcessing()) {
                    inputNode->process(nodes, connections);
                }
                
                cv::Mat inputImage = inputNode->getOutputImage(outputPinIndex);
                if (!inputImage.empty()) {
                    // Convert to grayscale if necessary
                    cv::Mat grayImage;
                    if (inputImage.channels() > 1) {
                        cv::cvtColor(inputImage, grayImage, cv::COLOR_BGR2GRAY);
                    } else {
                        grayImage = inputImage.clone();
                    }
                    
                    // Apply edge detection based on selected algorithm
                    if (algorithm == 0) {  // Sobel
                        // Ensure ksize is odd
                        int ksize = (sobelKSize % 2 == 0) ? sobelKSize + 1 : sobelKSize;
                        
                        // Apply Sobel in X and Y directions
                        cv::Mat sobelX, sobelY, sobelCombined;
                        cv::Sobel(grayImage, sobelX, CV_16S, sobelDx, 0, ksize);
                        cv::Sobel(grayImage, sobelY, CV_16S, 0, sobelDy, ksize);
                        
                        // Convert back to CV_8U
                        cv::Mat absX, absY;
                        cv::convertScaleAbs(sobelX, absX);
                        cv::convertScaleAbs(sobelY, absY);
                        
                        // Combine the results
                        cv::addWeighted(absX, 0.5, absY, 0.5, 0, edgeImage);
                    } else {  // Canny
                        // Ensure aperture size is odd and between 3-7
                        int apertureSize = std::max(3, std::min(7, (cannyApertureSize % 2 == 0) ? 
                                                cannyApertureSize + 1 : cannyApertureSize));
                                                
                        cv::Canny(grayImage, edgeImage, cannyThreshold1, cannyThreshold2, apertureSize);
                    }
                    
                    // Handle overlay option
                    if (overlay && inputImage.channels() == 3) {
                        // Create a 3-channel version of the edge image
                        cv::Mat edgeColor;
                        cv::cvtColor(edgeImage, edgeColor, cv::COLOR_GRAY2BGR);
                        
                        // Overlay using weighted addition
                        cv::addWeighted(inputImage, 1.0 - overlayWeight, edgeColor, overlayWeight, 0, processedImage);
                    } else if (overlay && inputImage.channels() == 1) {
                        // For grayscale input, we can just blend directly
                        cv::addWeighted(inputImage, 1.0 - overlayWeight, edgeImage, overlayWeight, 0, processedImage);
                    } else {
                        // No overlay, just use edge image
                        processedImage = edgeImage.clone();
                    }
                    
                    updatePreviewTexture();
                    markProcessed();
                }
            }
        }
    
        void drawNodeContent() override {
            // Input pin
            ImNodes::BeginInputAttribute(inputPins[0]);
            ImGui::Text("Input");
            ImNodes::EndInputAttribute();
    
            // Node controls
            bool changed = false;
            
            // Algorithm selection
            const char* algorithms[] = { "Sobel", "Canny" };
            changed |= ImGui::Combo("Algorithm", &algorithm, algorithms, IM_ARRAYSIZE(algorithms));
            
            // Parameter controls based on selected algorithm
            if (algorithm == 0) {  // Sobel
                changed |= ImGui::SliderInt("Kernel Size", &sobelKSize, 1, 7, "%d");
                ImGui::Text("(Must be odd, 1-7)");
                
                changed |= ImGui::SliderInt("X Derivative", &sobelDx, 0, 2);
                changed |= ImGui::SliderInt("Y Derivative", &sobelDy, 0, 2);
                ImGui::Text("(At least one derivative order must be > 0)");
                
                // Ensure at least one derivative is non-zero
                if (sobelDx == 0 && sobelDy == 0) {
                    sobelDx = 1;
                    changed = true;
                }
            } else {  // Canny
                changed |= ImGui::SliderInt("Lower Threshold", &cannyThreshold1, 0, 255);
                changed |= ImGui::SliderInt("Upper Threshold", &cannyThreshold2, 0, 255);
                changed |= ImGui::SliderInt("Aperture Size", &cannyApertureSize, 3, 7, "%d");
                ImGui::Text("(Must be odd, 3-7)");
                
                // Ensure threshold2 >= threshold1
                if (cannyThreshold2 < cannyThreshold1) {
                    cannyThreshold2 = cannyThreshold1;
                    changed = true;
                }
            }
            
            // Overlay option
            changed |= ImGui::Checkbox("Overlay on Original", &overlay);
            
            if (overlay) {
                changed |= ImGui::SliderFloat("Overlay Weight", &overlayWeight, 0.0f, 1.0f);
            }
            
            if (changed) {
                setNeedsProcessing();
            }
            
            // Display edge preview
            if (previewTexture) {
                ImGui::Text("Edge Preview:");
                ImGui::Image(reinterpret_cast<ImTextureID>(static_cast<unsigned long long int>(previewTexture)), 
                             ImVec2(150, 150));
            }
    
            // Output pin
            ImNodes::BeginOutputAttribute(outputPins[0]);
            ImGui::Text("Output");
            ImNodes::EndOutputAttribute();
        }
    };

// Node Editor class to manage the canvas
class NodeEditor {
private:
    std::map<int, std::shared_ptr<Node>> nodes;
    std::vector<Connection> connections;
    int nextNodeId = 1;

public:
    NodeEditor() {
        ImNodes::CreateContext();
        ImNodes::StyleColorsDark();
    }

    ~NodeEditor() {
        ImNodes::DestroyContext();
    }

    // Add to NodeEditor class public section:
    const std::map<int, std::shared_ptr<Node>>& getNodes() const {
        return nodes;
    }

    void update() {
        // Process all output nodes, which will trigger the processing chain
        for (auto& nodePair : nodes) {
            auto node = nodePair.second;
            if (dynamic_cast<OutputNode*>(node.get()) != nullptr) {
                node->process(nodes, connections);
            }
        }
    }

    void render() {
        ImNodes::BeginNodeEditor();

        // Draw all nodes
        for (auto& nodePair : nodes) {
            auto node = nodePair.second;
            ImNodes::BeginNode(node->getId());
            
            ImGui::TextUnformatted(node->getName().c_str());
            ImGui::Separator();
            
            node->drawNodeContent();
            
            ImNodes::EndNode();
        }

        // Draw connections
        for (const auto& connection : connections) {
            ImNodes::Link(
                connection.inputNodeId * 1000000 + connection.outputNodeId, 
                connection.outputPinId, 
                connection.inputPinId
            );
        }

        ImNodes::EndNodeEditor();

        // Handle new connections
        int startPinId, endPinId;
        if (ImNodes::IsLinkCreated(&startPinId, &endPinId)) {
            // Find which nodes these pins belong to
            int outputNodeId = -1, inputNodeId = -1;
            for (const auto& nodePair : nodes) {
                for (int pinId : nodePair.second->getOutputPins()) {
                    if (pinId == startPinId) {
                        outputNodeId = nodePair.first;
                    }
                }
                for (int pinId : nodePair.second->getInputPins()) {
                    if (pinId == endPinId) {
                        inputNodeId = nodePair.first;
                    }
                }
            }

            if (outputNodeId != -1 && inputNodeId != -1) {
                // Remove any existing connection to this input pin
                connections.erase(
                    std::remove_if(
                        connections.begin(), 
                        connections.end(),
                        [endPinId, inputNodeId](const Connection& conn) {
                            return conn.inputPinId == endPinId && conn.inputNodeId == inputNodeId;
                        }
                    ),
                    connections.end()
                );

                // Add the new connection
                connections.emplace_back(inputNodeId, outputNodeId, endPinId, startPinId);
                
                // Mark all nodes for reprocessing
                for (auto& nodePair : nodes) {
                    nodePair.second->setNeedsProcessing();
                }
            }
        }

        // Handle deleted connections
        int linkId;
        if (ImNodes::IsLinkDestroyed(&linkId)) {
            for (auto it = connections.begin(); it != connections.end(); ++it) {
                if (it->inputNodeId * 1000000 + it->outputNodeId == linkId) {
                    connections.erase(it);
                    
                    // Mark all nodes for reprocessing
                    for (auto& nodePair : nodes) {
                        nodePair.second->setNeedsProcessing();
                    }
                    break;
                }
            }
        }
    }

    void addNode(const std::shared_ptr<Node>& node) {
        nodes[node->getId()] = node;
    }

    std::shared_ptr<Node> createNode(const std::string& type) {
        int id = nextNodeId++;
        
        if (type == "ImageInput") {
            return std::make_shared<ImageInputNode>(id);
        }
        else if (type == "BrightnessContrast") {
            return std::make_shared<BrightnessContrastNode>(id);
        }
        else if (type == "Output") {
            return std::make_shared<OutputNode>(id);
        }
        else if (type == "ColorSplitter") {
            return std::make_shared<ColorChannelSplitterNode>(id);
        }        
        else if (type == "Blur") {
            return std::make_shared<BlurNode>(id);
        }
        else if (type == "Threshold") {
            return std::make_shared<ThresholdNode>(id);
        }
        else if (type == "EdgeDetection") {
            return std::make_shared<EdgeDetectionNode>(id);
        }
        
        return nullptr;
    }
};

// Main application
int main() {
    // Setup GLFW and ImGui
    if (!glfwInit()) {
        return -1;
    }

    GLFWwindow* window = glfwCreateWindow(1280, 720, "Node-Based Image Processor", NULL, NULL);
    if (!window) {
        glfwTerminate();
        return -1;
    }

    glfwMakeContextCurrent(window);
    glfwSwapInterval(1);  // Enable vsync

    // Initialize ImGui
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO(); (void)io;

    ImGui::StyleColorsDark();
    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL3_Init("#version 130");

    // Create node editor
    NodeEditor editor;
    
    // Main loop
    while (!glfwWindowShouldClose(window)) {
        glfwPollEvents();

        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        // Main window
        ImGui::SetNextWindowPos(ImVec2(0, 0));
        ImGui::SetNextWindowSize(ImGui::GetIO().DisplaySize);
        ImGui::Begin("Node Editor", nullptr, ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_MenuBar);

        // Menu bar
        if (ImGui::BeginMenuBar()) {
            // if (ImGui::BeginMenu("File")) {
            //     ImGui::EndMenu();
            // }
            if (ImGui::BeginMenu("Add Node")) {
                if (ImGui::MenuItem("Image Input")) {
                    auto node = editor.createNode("ImageInput");
                    editor.addNode(node);
                }
                if (ImGui::MenuItem("Brightness/Contrast")) {
                    auto node = editor.createNode("BrightnessContrast");
                    editor.addNode(node);
                }
                if (ImGui::MenuItem("Output")) {
                    auto node = editor.createNode("Output");
                    editor.addNode(node);
                }
                if (ImGui::MenuItem("Color Splitter")) {
                    auto node = editor.createNode("ColorSplitter");
                    editor.addNode(node);
                }                
                if (ImGui::MenuItem("Blur")) {
                    auto node = editor.createNode("Blur");
                    editor.addNode(node);
                }
                if (ImGui::MenuItem("Threshold")) {
                    auto node = editor.createNode("Threshold");
                    editor.addNode(node);
                }
                if (ImGui::MenuItem("Edge Detection")) {
                    auto node = editor.createNode("EdgeDetection");
                    editor.addNode(node);
                }
                ImGui::EndMenu();
            }
            ImGui::EndMenuBar();
        }

        // Update processing chain
        editor.update();
        
        // Render node editor
        editor.render();

        ImGui::End();

        // Rendering
        ImGui::Render();
        int display_w, display_h;
        glfwGetFramebufferSize(window, &display_w, &display_h);
        glViewport(0, 0, display_w, display_h);
        glClearColor(0.2f, 0.2f, 0.2f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

        glfwSwapBuffers(window);
    }

    // Cleanup
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();

    glfwDestroyWindow(window);
    glfwTerminate();

    return 0;
}