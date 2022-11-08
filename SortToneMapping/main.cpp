#include <miniz/miniz.h>
#define TINYEXR_USE_MINIZ
#define TINYEXR_IMPLEMENTATION
#include <tinyexr/tinyexr.h>
#define STB_IMAGE_IMPLEMENTATION
#include <stb/stb_image.h>
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include <stb/stb_image_write.h>

#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <iostream>
#include <algorithm>
#include <array>
#include <cstdlib>
#include <random>

#include "shader.h"

#include <imgui/imgui_impl_glfw.h>
#include <imgui/imgui_impl_opengl3.h>

#include <imgui/widgets/bezier.hpp>
#include <imgui/widgets/curve-editor-lumix.hpp>
#include <imgui/widgets/curve-editor.hpp>


void framebuffer_size_callback(GLFWwindow* window, int width, int height);
void processInput(GLFWwindow* window);

// settings
const unsigned int SCR_WIDTH = 800;
const unsigned int SCR_HEIGHT = 600;

struct Pixel {
    uint8_t r, g, b;
    uint8_t v;
    int i, j;
};

void swap(uint8_t* a, uint8_t* b)
{
    uint8_t t = *a;
    *a = *b;
    *b = t;
}

int partition(uint8_t*& arr, int l, int h)
{
    int x = arr[h];
    int i = (l - 1);

    for (int j = l; j <= h - 1; j++) {
        if (arr[j] <= x) {
            i++;
            swap(&arr[i], &arr[j]);
        }
    }
    swap(&arr[i + 1], &arr[h]);
    return (i + 1);
}

void quickSortIterative(uint8_t*& arr, int l, int h)
{
    int* stack = new int[h - l + 1];
    int top = -1;
    stack[++top] = l;
    stack[++top] = h;

    while (top >= 0) {
        h = stack[top--];
        l = stack[top--];

        int p = partition(arr, l, h);

        if (p - 1 > l) {
            stack[++top] = l;
            stack[++top] = p - 1;
        }

        if (p + 1 < h) {
            stack[++top] = p + 1;
            stack[++top] = h;
        }
    }
    free(stack);
}

void constructImage(Pixel* image, uint8_t* rgb, int w, int h) {
    for (int i = 0; i < w; i++) {
        for (int j = 0; j < h; j++) {
            rgb[(i * h + j) * 3] = image[i * h + j].r;
            rgb[(i * h + j) * 3 + 1] = image[i * h + j].g;
            rgb[(i * h + j) * 3 + 2] = image[i * h + j].b;
        }
    }
}

void updateHistogram(uint8_t* image, int w, int h, int nCh, float* hist) {
    for (int i = 0; i < w; i++) {
        for (int j = 0; j < h; j++) {
            hist[image[(i * h + j) * nCh]] = 0.0f;
        }
    }
    for (int i = 0; i < w; i++) {
        for (int j = 0; j < h; j++) {
            hist[image[(i * h + j) * nCh]] += 1.0f;
        }
    }
}

int main() {
    //std::random_device rd;     // Only used once to initialise (seed) engine
    //std::mt19937 rng(rd());    // Random-number engine used (Mersenne-Twister in this case)
    //std::uniform_int_distribution<int> uni(min, max); // Guaranteed unbiased

    //auto random_integer = uni(rng);
    const char* input = "C:\\Users\\Tolga YILDIZ\\SortToneMapping\\SortToneMapping\\renders\\03.exr";
    float* out = NULL; // width * height * RGBA
    float* bw = NULL; // width * height * V
    float hist[256] = { 0.0f };
    float rHist[256] = { 0.0f };
    float gHist[256] = { 0.0f };
    float bHist[256] = { 0.0f };
    uint8_t* pixVal = NULL;
    //uint8_t* pixValSorted;
    int width;
    int height;
    const char* err = NULL; // or nullptr in C++11

    EXRVersion exrVersion;
    int ret = ParseEXRVersionFromFile(&exrVersion, input);
    if (ret != 0) {
        fprintf(stderr, "Invalid EXR file: %s\n", input);
        return -1;
    }
    printf("EXR Version: %i\n", exrVersion.version);

    if (exrVersion.multipart) {
        printf("Multi Part EXR discarded\n");
        return -1;
    }

    EXRHeader exrHeader;
    InitEXRHeader(&exrHeader);

    ret = ParseEXRHeaderFromFile(&exrHeader, &exrVersion, input, &err);
    if (ret != 0) {
        fprintf(stderr, "Parse EXR ERR: %s\n", err);
        FreeEXRErrorMessage(err);
        return ret;
    }
    printf("EXR Header: \n\tNum Channel: %i\n\tAspect Ratio:%f\n", exrHeader.num_channels, exrHeader.pixel_aspect_ratio);
    EXRImage exrImage;
    InitEXRImage(&exrImage);

    ret = LoadEXRImageFromFile(&exrImage, &exrHeader, input, &err);
    if (ret != 0) {
        fprintf(stderr, "Load EXR ERR: %s\n", input);
        FreeEXRErrorMessage(err);
        FreeEXRHeader(&exrHeader);
        return ret;
    }


    ret = LoadEXR(&out, &width, &height, input, &err);
    
    if (ret != TINYEXR_SUCCESS) {
        if (err) {
            fprintf(stderr, "ERR : %s\n", err);
            FreeEXRErrorMessage(err); // release memory of error message.
            return -1;
        }
    }
    printf("Image size: (%i, %i)\n", width, height);
    Pixel* image = new Pixel[width * height];
    bw = new float[width * height];
    pixVal = new uint8_t[width * height * 3];
    //pixValSorted = new uint8_t[width * height * 4];
    // find max
    float max = 0.0f;
    for (int i = 0; i < width * height * exrHeader.num_channels; i++) {
        if (out[i] > max)
            max = out[i];
    }
    // normalize between 0.0f and 1.0f, these are still 32 bit floats
    for (int i = 0; i < width * height * exrHeader.num_channels; i++) {
        out[i] /= max;
    }
    // get the pixValue on 256 different values
    for (int i = 0; i < height; i++) {
        for (int j = 0; j < width; j++) {
            bw[i * width + j] = sqrt(out[i * 4 * width + j * 4] * out[i * 4 * width + j * 4] +
                out[i * 4 * width + j * 4 + 1] * out[i * 4 * width + j * 4 + 1] +
                out[i * 4 * width + j * 4 + 2] * out[i * 4 * width + j * 4 + 2]);
            pixVal[i * width * 3 + j * 3] = static_cast<uint8_t>(255 * out[i * width * 4 + j * 4]);//static_cast<uint8_t>(256 * bw[i * width + j]);
            pixVal[i * width * 3 + j * 3 + 1] = static_cast<uint8_t>(255 * out[i * width * 4 + j * 4 + 1]);
            pixVal[i * width * 3 + j * 3 + 2] = static_cast<uint8_t>(255 * out[i * width * 4 + j * 4 + 2]);
            hist[static_cast<uint8_t>(255 * bw[i * width + j])] += 1.0f;
            rHist[pixVal[i * width * 3 + j * 3]] += 1.0f;
            gHist[pixVal[i * width * 3 + j * 3 + 1]] += 1.0f;
            bHist[pixVal[i * width * 3 + j * 3 + 2]] += 1.0f;
            //pixVal[i * width * 4 + j * 4 + 3] = static_cast<uint8_t>(255 * out[i * width * 4 + j * 4 + 3]);

            //printf("RGBA of pixel (%i,%i): [%f,%f,%f,%f]\n", i, j, out[i * 4 * width + j * 4], out[i * 4 * width + j * 4 + 1], out[i * 4 * width + j * 4 + 2], out[i * 4 * width + j * 4 + 3]);

        }
    }
    printf("RGBA of pixel (%i,%i): [%f,%f,%f,%f]\n", 0, 0, out[0], out[1], out[2], out[3]);
    printf("Value of pixel (%i,%i): [%f]\n", 0, 0, bw[0]);
    printf("Integer Value of pixel (%i,%i): [%i]\n", 0, 0, pixVal[0]);
    //std::sort(pixVal, pixVal + width * height * 3);
    /*
    for (int i = 0; i < width * height * 3; i++) {
        for (int j = i + 1; j < width * height * 3; j++) {
            if (pixVal[i] > pixVal[j]) {
                uint8_t temp = pixVal[i];
                pixVal[i] = pixVal[j];
                pixVal[j] = temp;
            }
        }
    }*/
    //quickSortIterative(pixVal, 0, width * height * 3 - 1);
    
    stbi_write_png("result.png", width, height, 1, pixVal, width);
    
    //set glfw
    glfwInit();
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

    GLFWwindow* window = glfwCreateWindow(width, height, "Edit EXR", NULL, NULL);
    if (window == NULL) {
        printf("Failed to create GLFW Window\n");
        glfwTerminate();
        return -1;
    }
    glfwMakeContextCurrent(window);
    glfwWindowHint(GLFW_RESIZABLE, GL_FALSE);
    glfwSetFramebufferSizeCallback(window, framebuffer_size_callback);

    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress)) {
        printf("Failed to init GLAD");
        return -1;
    }

    Shader texShader = Shader("C:\\Users\\Tolga YILDIZ\\SortToneMapping\\SortToneMapping\\shaders\\vertex.glsl","C:\\Users\\Tolga YILDIZ\\SortToneMapping\\SortToneMapping\\shaders\\frag.glsl");
    
    float vertices[] = {
        // positions          // colors           // texture coords
         1.0f,  1.f, 0.0f,   1.0f, 1.0f, 1.0f,   1.0f, 0.0f, // top right
         1.f, -1.f, 0.0f,   0.0f, 1.0f, 0.0f,   1.0f, 1.0f, // bottom right
        -1.f, -1.f, 0.0f,   0.0f, 0.0f, 1.0f,   0.0f, 1.0f, // bottom left
        -1.f,  1.f, 0.0f,   1.0f, 1.0f, 0.0f,   0.0f, 0.0f  // top left 
    };
    unsigned int indices[] = {
        0, 1, 3, // first triangle
        1, 2, 3  // second triangle
    };
    unsigned int VBO, VAO, EBO;
    glGenVertexArrays(1, &VAO);
    glGenBuffers(1, &VBO);
    glGenBuffers(1, &EBO);

    glBindVertexArray(VAO);

    glBindBuffer(GL_ARRAY_BUFFER, VBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, EBO);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(indices), indices, GL_STATIC_DRAW);

    // position attribute
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);
    // color attribute
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)(3 * sizeof(float)));
    glEnableVertexAttribArray(1);
    // texture coord attribute
    glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)(6 * sizeof(float)));
    glEnableVertexAttribArray(2);

    // read exr image
    

    unsigned int texture1, texture2;
    // texture 1
    // ---------
    glGenTextures(1, &texture1);
    glBindTexture(GL_TEXTURE_2D, texture1);
    // set the texture wrapping parameters
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);	// set texture wrapping to GL_REPEAT (default wrapping method)
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    // set texture filtering parameters
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    // load image, create texture and generate mipmaps
    int nrChannels;
    stbi_set_flip_vertically_on_load(true); // tell stb_image.h to flip loaded texture's on the y-axis.
    // The FileSystem::getPath(...) is part of the GitHub repository so we can find files on any IDE/platform; replace it with your own image path.
    //std::string path = "C:\\Users\\Tolga YILDIZ\\SortToneMapping\\SortToneMapping\\images\\Tiles\\GoldenGate.exr";
    //unsigned char* data = (path.c_str(), & width, & height, & nrChannels, 0);
    if (pixVal)
    {
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, width, height, 0, GL_RGB, GL_UNSIGNED_BYTE, pixVal);
        glGenerateMipmap(GL_TEXTURE_2D);
    }
    else
    {
        std::cout << "Failed to load texture" << std::endl;
    }

    texShader.use(); // don't forget to activate/use the shader before setting uniforms!
    // either set it manually like so:
    glUniform1i(glGetUniformLocation(texShader.ID, "texture1"), 0);
    // or set it via the texture class
    //texShader.setInt("texture2", 1);

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO(); (void)io;
    ImGui::StyleColorsDark();
    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL3_Init("#version 330");

    io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;

    ImVec2 p[4] = { {0.0f,0.0f},{0.1f,0.2f},{0.3f,0.5f},{1.0f,1.0f} };
    float histScale = width * height/10.0f;
    while (!glfwWindowShouldClose(window))
    {
        // input
        // -----
        processInput(window);

        // render
        // ------
        glClearColor(0.2f, 0.3f, 0.3f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);

        //start of imgui init stuff
        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        //end of imgui init stuff
        // bind textures on corresponding texture units
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, texture1);

        // render container
        texShader.use();
        glBindVertexArray(VAO);
        glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0);

        // glfw: swap buffers and poll IO events (keys pressed/released, mouse moved etc.)
        // -------------------------------------------------------------------------------
        /*
        ImGui::Begin("UI");
        static float v[5] = { 0.950f, 0.050f, 0.795f, 0.035f };
        ImGui::Bezier("Bezier", v);
        static float x[5] = { 0.950f, 0.050f, 0.795f, 0.035f };
        ImGui::BezierValue(1.0f,x);
        ImVec2 vec1[1] = { ImVec2() };
        ImVec2 vec2[1] = { ImVec2() };
        //ImGui::bezier_table(vec1, vec2);
        ImGui::End();

        ImGui::Begin("Curve Editor");
        ImGui::CurveEditor("Curve Editor",v, 20, ImVec2(400.0f, 400.f),ImU32(), nullptr);
        ImGui::End();
        */
        ImGui::Begin("Curve Editor2");
        ImGui::Curve("curve editor2", ImVec2(400,400), 4, p);
        ImGui::CurveValue(0.1, 4, p);
        ImGui::End();

        ImGui::Begin("Image Histogram");
        updateHistogram(pixVal, width, height, 3, hist);
        ImGui::SliderFloat("Scale Value", &histScale, width * height/100.0f, width * height);
        ImGui::PlotHistogram("Value Histogram", hist, 256, 0, NULL, 0.0f, histScale, ImVec2(0, 200.f), sizeof(float), ImVec4(1.0f, 1.0f, 1.0f, 1.0f));// , ImVec4(1.0f, 0.0f, 0.0f, 1.0f));
        ImGui::PlotHistogram("Value Histogram", rHist, 256, 0, NULL, 0.0f, histScale, ImVec2(0, 200.f), sizeof(float), ImVec4(1.0f, 0.0f, 0.0f, 1.0f));// , ImVec4(1.0f, 0.0f, 0.0f, 1.0f));
        ImGui::PlotHistogram("Value Histogram", gHist, 256, 0, NULL, 0.0f, histScale, ImVec2(0, 200.f), sizeof(float), ImVec4(0.0f, 1.0f, 0.0f, 1.0f));// , ImVec4(1.0f, 0.0f, 0.0f, 1.0f));
        ImGui::PlotHistogram("Value Histogram", bHist, 256, 0, NULL, 0.0f, histScale, ImVec2(0, 200.f), sizeof(float), ImVec4(0.0f, 0.0f, 1.0f, 1.0f));// , ImVec4(1.0f, 0.0f, 0.0f, 1.0f));
        ImGui::End();
        ImGui::Render();
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
        glfwSwapBuffers(window);
        glfwPollEvents();
    }
    //stbi_image_free(data);
    free(out); // release memory of image data
    free(bw);
    free(pixVal);

    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();

    glDeleteVertexArrays(1, &VAO);
    glDeleteBuffers(1, &VBO);
    glDeleteBuffers(1, &EBO);

    glfwTerminate();
    return 0;
}

void processInput(GLFWwindow* window)
{
    if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS)
        glfwSetWindowShouldClose(window, true);
}

// glfw: whenever the window size changed (by OS or user resize) this callback function executes
// ---------------------------------------------------------------------------------------------
void framebuffer_size_callback(GLFWwindow* window, int width, int height)
{
    // make sure the viewport matches the new window dimensions; note that width and 
    // height will be significantly larger than specified on retina displays.
    glViewport(0, 0, width, height);
}