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
                inputImage.convertTo(processedImage, -1, contrast, brightness * 2.55f);
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
            if (ImGui::BeginMenu("File")) {
                ImGui::EndMenu();
            }
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