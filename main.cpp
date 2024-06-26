// Dear ImGui: standalone example application for GLFW + OpenGL 3, using programmable pipeline
// (GLFW is a cross-platform general purpose library for handling windows, inputs, OpenGL/Vulkan/Metal graphics context creation, etc.)

// Learn about Dear ImGui:
// - FAQ                  https://dearimgui.com/faq
// - Getting Started      https://dearimgui.com/getting-started
// - Documentation        https://dearimgui.com/docs (same as your local docs/ folder).
// - Introduction, links and more at the top of imgui.cpp

#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl3.h"
#include <stdio.h>
#include <iostream>
#include <string>
#include <fstream>
#include <cstdlib>
#include <array>
#include <map>
#include <vector>
#include <filesystem>
#define STB_IMAGE_IMPLEMENTATION
#include "headers/stb_image.h"
#define GL_SILENCE_DEPRECATION
#if defined(IMGUI_IMPL_OPENGL_ES2)
#include <GLES2/gl2.h>
#endif
#include <GLFW/glfw3.h> // Will drag system OpenGL headers

// [Win32] Our example includes a copy of glfw3.lib pre-compiled with VS2010 to maximize ease of testing and compatibility with old VS compilers.
// To link with VS2010-era libraries, VS2015+ requires linking with legacy_stdio_definitions.lib, which we do using this pragma.
// Your own project should not be affected, as you are likely to link with a newer binary of GLFW that is adequate for your version of Visual Studio.
#if defined(_MSC_VER) && (_MSC_VER >= 1900) && !defined(IMGUI_DISABLE_WIN32_FUNCTIONS)
#pragma comment(lib, "legacy_stdio_definitions")
#endif

// This example can also compile and run with Emscripten! See 'Makefile.emscripten' for details.
#ifdef __EMSCRIPTEN__
#include "../libs/emscripten/emscripten_mainloop_stub.h"
#endif

static void glfw_error_callback(int error, const char* description)
{
    fprintf(stderr, "GLFW Error %d: %s\n", error, description);
}

/* The following function is from ocornut's (Dear ImGUI creator) guide on loading images with ImGUI */
/* https://github.com/ocornut/imgui/wiki/Image-Loading-and-Displaying-Examples#index */
// Simple helper function to load an image into a OpenGL texture with common settings
bool LoadTextureFromFile(const char* filename, GLuint* out_texture, int* out_width, int* out_height)
{
    // Load from file
    int image_width = 0;
    int image_height = 0;
    
    unsigned char* image_data = stbi_load(filename, &image_width, &image_height, NULL, 4);
    if (image_data == NULL)
        return false;

    // Create a OpenGL texture identifier
    GLuint image_texture;
    glGenTextures(1, &image_texture);
    glBindTexture(GL_TEXTURE_2D, image_texture);

    // Setup filtering parameters for display
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE); // This is required on WebGL for non power-of-two textures
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE); // Same

    // Upload pixels into texture
#if defined(GL_UNPACK_ROW_LENGTH) && !defined(__EMSCRIPTEN__)
    glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);
#endif
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, image_width, image_height, 0, GL_RGBA, GL_UNSIGNED_BYTE, image_data);
    stbi_image_free(image_data);

    *out_texture = image_texture;
    *out_width = image_width;
    *out_height = image_height;

    return true;
}

// callback function to resize the string buffer (from demo code)
static int MyResizeCallback(ImGuiInputTextCallbackData* data)
{
    if (data->EventFlag == ImGuiInputTextFlags_CallbackResize)
    {
        ImVector<char>* my_str = (ImVector<char>*)data->UserData;
        IM_ASSERT(my_str->begin() == data->Buf);
        my_str->resize(data->BufSize); // NB: On resizing calls, generally data->BufSize == data->BufTextLen + 1
        data->Buf = my_str->begin();
    }
    return 0;
}

// from the demo code
static bool MyInputTextMultiline(const char* label, ImVector<char>* my_str, const ImVec2& size = ImVec2(0, 0), ImGuiInputTextFlags flags = 0)
{
    IM_ASSERT((flags & ImGuiInputTextFlags_CallbackResize) == 0);

    flags |= ImGuiInputTextFlags_CallbackResize | ImGuiInputTextFlags_AllowTabInput;

    return ImGui::InputTextMultiline(label, my_str->begin(), (size_t)my_str->size(), size, flags | ImGuiInputTextFlags_CallbackResize, MyResizeCallback, (void*)my_str);
}

static bool MyInputText(const char* label, ImVector<char>* my_str, const ImVec2& size = ImVec2(0, 0), ImGuiInputTextFlags flags = 0)
{
    IM_ASSERT((flags & ImGuiInputTextFlags_CallbackResize) == 0);

    flags |= ImGuiInputTextFlags_CallbackResize;

    return ImGui::InputText(label, my_str->begin(), (size_t)my_str->size(), flags | ImGuiInputTextFlags_CallbackResize, MyResizeCallback, (void*)my_str);
}

static void SaveToFile(std::string filename, std::string theText)
{
    // make sure to append a new line to the end of the file text
    // otherwise errors will occur
    if (!theText.empty() && theText.back() != '\n') {
        theText += '\n';
    }

    std::ofstream OutFile(filename);

    OutFile << theText;

    OutFile.close();
}

static ImVector<char> OpenFile(std::string filename)
{
    std::string fileLine;

    std::ifstream InFile(filename);

    ImVector<char> outputText;
    char byte;
    while (InFile.get(byte)) {
        outputText.push_back(byte);
    }

    InFile.close();
    return outputText;
}

static std::string FileNameWithoutDot(const std::string& str)
{
    size_t dotPos = str.find_last_of('.');
    if (dotPos != std::string::npos) {
        return str.substr(0, dotPos);
    }
    return str;
}


static std::string RunConsoleCommand(std::string command) {
    FILE* pipe = popen(command.c_str(), "r");
    if (!pipe) {
        std::cerr << "Error: Unable to open pipe." << std::endl;
        return "err";
    }

    std::string newConsoleOutputText = "";

    std::array<char, 128> buffer;
    while (fgets(buffer.data(), buffer.size(), pipe) != nullptr) {
        //std::cout << buffer.data();
        newConsoleOutputText += buffer.data();
    }

    pclose(pipe);

    std::string searchString = "g++";

    if (command.find(searchString) != std::string::npos) {
        newConsoleOutputText = "Compiled successfully.";
    }

    return newConsoleOutputText;
}

static std::map<int, ImVector<char>> indexed_im_vectors;

static void AddIndexedImVector(int index, const ImVector<char>& im_vector) {
    indexed_im_vectors[index] = im_vector;
}

static ImVector<char>& GetIndexedImVector(int index) {
    return indexed_im_vectors[index];
}

static void RemoveIndexedImVector(int index) {
    indexed_im_vectors.erase(index);

    // Recalculate indexes
    int new_index = 0;
    std::map<int, ImVector<char>> new_map;
    for (const auto& pair : indexed_im_vectors) {
        new_map[new_index++] = pair.second;
    }
    indexed_im_vectors = new_map;
}

// global variables

static std::string currentFile = "no file opened";
static std::string consoleOutputText = "";
static ImVector<std::string> tab_names;
static ImVector<int> active_tabs;
static int next_tab_id = 0;
std::string displayedDir = "";
static ImVector<char> dir_name;
std::string currentDirectory = "";

// Note that because we need to store a terminating zero character, our size/capacity are 1 more
// than usually reported by a typical string class.
static ImVector<char> my_str;

std::__fs::filesystem::path absolute_path = std::__fs::filesystem::absolute("irohde");
std::string absPath = "Absolute path to irohDE directory: " + absolute_path.string();

// Main code
int main(int, char**)
{
    glfwSetErrorCallback(glfw_error_callback);
    if (!glfwInit())
        return 1;

    // Decide GL+GLSL versions
#if defined(IMGUI_IMPL_OPENGL_ES2)
    // GL ES 2.0 + GLSL 100
    const char* glsl_version = "#version 100";
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 2);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 0);
    glfwWindowHint(GLFW_CLIENT_API, GLFW_OPENGL_ES_API);
#elif defined(__APPLE__)
    // GL 3.2 + GLSL 150
    const char* glsl_version = "#version 150";
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 2);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);  // 3.2+ only
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);            // Required on Mac
#else
    // GL 3.0 + GLSL 130
    const char* glsl_version = "#version 130";
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 0);
    //glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);  // 3.2+ only
    //glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);            // 3.0+ only
#endif

    // Create window with graphics context
    GLFWwindow* window = glfwCreateWindow(1280, 720, "IrohDE", nullptr, nullptr);
    if (window == nullptr)
        return 1;
    glfwMakeContextCurrent(window);
    glfwSwapInterval(1); // Enable vsync

    // Setup Dear ImGui context
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO(); (void)io;
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;     // Enable Keyboard Controls
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;      // Enable Gamepad Controls

    // Setup Dear ImGui style
    ImGui::StyleColorsDark();
    //ImGui::StyleColorsLight();

    // Setup Platform/Renderer backends
    ImGui_ImplGlfw_InitForOpenGL(window, true);
#ifdef __EMSCRIPTEN__
    ImGui_ImplGlfw_InstallEmscriptenCanvasResizeCallback("#canvas");
#endif
    ImGui_ImplOpenGL3_Init(glsl_version);

    // Load Fonts
    // - If no fonts are loaded, dear imgui will use the default font. You can also load multiple fonts and use ImGui::PushFont()/PopFont() to select them.
    // - AddFontFromFileTTF() will return the ImFont* so you can store it if you need to select the font among multiple.
    // - If the file cannot be loaded, the function will return a nullptr. Please handle those errors in your application (e.g. use an assertion, or display an error and quit).
    // - The fonts will be rasterized at a given size (w/ oversampling) and stored into a texture when calling ImFontAtlas::Build()/GetTexDataAsXXXX(), which ImGui_ImplXXXX_NewFrame below will call.
    // - Use '#define IMGUI_ENABLE_FREETYPE' in your imconfig file to use Freetype for higher quality font rendering.
    // - Read 'docs/FONTS.md' for more instructions and details.
    // - Remember that in C/C++ if you want to include a backslash \ in a string literal you need to write a double backslash \\ !
    // - Our Emscripten build process allows embedding fonts to be accessible at runtime from the "fonts/" folder. See Makefile.emscripten for details.
    //io.Fonts->AddFontDefault();
    //io.Fonts->AddFontFromFileTTF("c:\\Windows\\Fonts\\segoeui.ttf", 18.0f);
    //io.Fonts->AddFontFromFileTTF("../../misc/fonts/DroidSans.ttf", 16.0f);
    //io.Fonts->AddFontFromFileTTF("../../misc/fonts/Roboto-Medium.ttf", 16.0f);
    //io.Fonts->AddFontFromFileTTF("../../misc/fonts/Cousine-Regular.ttf", 15.0f);
    //ImFont* font = io.Fonts->AddFontFromFileTTF("c:\\Windows\\Fonts\\ArialUni.ttf", 18.0f, nullptr, io.Fonts->GetGlyphRangesJapanese());
    //IM_ASSERT(font != nullptr);

    // Our state
    bool show_demo_window = false;
    bool show_another_window = false;
    ImVec4 clear_color = ImVec4(0.45f, 0.55f, 0.60f, 1.00f);

    // for file editing background
    int my_image_width = 0;
    int my_image_height = 0;
    GLuint my_image_texture = 0;
    bool ret = LoadTextureFromFile("images/uncle_iroh.png", &my_image_texture, &my_image_width, &my_image_height);
    IM_ASSERT(ret);

    // for console background
    int my_image_width2 = 0;
    int my_image_height2 = 0;
    GLuint my_image_texture2 = 0;
    bool ret2 = LoadTextureFromFile("images/iroh-tea.png", &my_image_texture2, &my_image_width2, &my_image_height2);
    IM_ASSERT(ret2);

    // Main loop
#ifdef __EMSCRIPTEN__
    // For an Emscripten build we are disabling file-system access, so let's not attempt to do a fopen() of the imgui.ini file.
    // You may manually call LoadIniSettingsFromMemory() to load settings from your own storage.
    io.IniFilename = nullptr;
    EMSCRIPTEN_MAINLOOP_BEGIN
#else
    while (!glfwWindowShouldClose(window))
#endif
    {
        // Poll and handle events (inputs, window resize, etc.)
        // You can read the io.WantCaptureMouse, io.WantCaptureKeyboard flags to tell if dear imgui wants to use your inputs.
        // - When io.WantCaptureMouse is true, do not dispatch mouse input data to your main application, or clear/overwrite your copy of the mouse data.
        // - When io.WantCaptureKeyboard is true, do not dispatch keyboard input data to your main application, or clear/overwrite your copy of the keyboard data.
        // Generally you may always pass all inputs to dear imgui, and hide them from your application based on those two flags.
        glfwPollEvents();

        // Start the Dear ImGui frame
        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        {
            ImGui::Begin("IrohDE");
            ImGui::Text("Editing: %s", currentFile.c_str());

            ImGui::SetCursorPosY(ImGui::GetCursorPosY() + 10.0f); // gives some space at the top
            ImGui::Image((void*)(intptr_t)my_image_texture, ImVec2(596, 335));

            ImGui::SetCursorPosY(ImGui::GetCursorPosY() - 350.0f); // makes the image appear behind the text input

            //ImGui::Text("Data: %p\nSize: %d\nCapacity: %d", (void*)my_str.begin(), my_str.size(), my_str.capacity());
            
            // if (next_tab_id == 0) // Initialize with some default tabs
            // {
            //     for (int i = 0; i < 3; i++)
            //     {
            //         active_tabs.push_back(next_tab_id);
            //         tab_names.push_back("Tab " + std::to_string(next_tab_id));
            //         next_tab_id++;
            //     }
            // }

            // start with 1 default tab
            // if (next_tab_id == 0)
            // {
            //     active_tabs.push_back(next_tab_id);
            //     tab_names.push_back("empty");
            //     next_tab_id++;
            // }

            static bool show_leading_button = true;
            static bool show_trailing_button = false;

            static ImGuiTabBarFlags tab_bar_flags = ImGuiTabBarFlags_AutoSelectNewTabs | ImGuiTabBarFlags_Reorderable | ImGuiTabBarFlags_FittingPolicyResizeDown;

            if (ImGui::BeginTabBar("MyTabBar", tab_bar_flags))
            {
                if (show_leading_button)
                    if (ImGui::TabItemButton("?", ImGuiTabItemFlags_Leading | ImGuiTabItemFlags_NoTooltip))
                        ImGui::OpenPopup("MyHelpMenu");
                if (ImGui::BeginPopup("MyHelpMenu"))
                {
                    ImGui::Selectable("Create or open a file and it will appear here.");
                    ImGui::EndPopup();
                }

                if (show_trailing_button)
                    if (ImGui::TabItemButton("+", ImGuiTabItemFlags_Trailing | ImGuiTabItemFlags_NoTooltip))
                    {
                        active_tabs.push_back(next_tab_id); // Add new tab
                        tab_names.push_back("New Tab " + std::to_string(next_tab_id));
                        next_tab_id++;
                    }

                for (int n = 0; n < active_tabs.Size; )
                {
                    bool open = true;
                    char name[16];
                    snprintf(name, IM_ARRAYSIZE(name), "%04d", active_tabs[n]);
                    if (ImGui::BeginTabItem(tab_names[n].c_str(), &open, ImGuiTabItemFlags_None))
                    {
                        // if (my_str.empty())
                        //     my_str.push_back(0);
                        currentFile = tab_names[n];
                        ImVector<char>& retrieved_vector = GetIndexedImVector(n);
                        MyInputTextMultiline("##MyStr", &retrieved_vector, ImVec2(-FLT_MIN, ImGui::GetTextLineHeight() * 16));
                        if (ImGui::Button("Save")) {
                            std::string newText;
                            if (!retrieved_vector.empty()) {
                                newText.assign(retrieved_vector.begin(), retrieved_vector.end());
                            }
                            // get rid of last char because it is an unsupported text format
                            if (!newText.empty()) {
                                newText.pop_back();
                            }
                            std::string filePath = currentDirectory.c_str() + currentFile;
                            //std::__fs::filesystem::path absolute_path = std::__fs::filesystem::absolute(currentFile.c_str());
                            //std::cout << "Opened file: " << currentFile.c_str() << " (absolute path: " << absolute_path << ")" << std::endl;
                            SaveToFile(filePath.c_str(), newText);
                        }
                        ImGui::EndTabItem();
                    }

                    if (!open)
                    {
                        active_tabs.erase(active_tabs.Data + n);
                        tab_names.erase(tab_names.Data + n);
                        RemoveIndexedImVector(n);
                        next_tab_id--;
                    }
                    else
                    {
                        n++;
                        
                    }
                }

                ImGui::EndTabBar();
            }

            ImGui::End();
        }

        {
            ImGui::Begin("Files");

            ImGui::Text("%s", absPath.c_str());

            ImGui::Text("Path to Directory: (Include '/' at end of path. Leave blank for current irohDE directory.)");
            if (dir_name.empty())
                dir_name.push_back(0);
            MyInputTextMultiline("##DirName", &dir_name, ImVec2(-FLT_MIN, ImGui::GetTextLineHeight() * 2));

            if (ImGui::Button("CD")) {
                currentDirectory = "";
                if (!dir_name.empty()) {
                    currentDirectory.assign(dir_name.begin(), dir_name.end());
                }
                std::string command = "ls " + currentDirectory;
                displayedDir = RunConsoleCommand(command);

                dir_name.clear();
            }

            ImGui::Text("File name:");
            static ImVector<char> file_name;
            if (file_name.empty())
                file_name.push_back(0);
            MyInputTextMultiline("##FileName", &file_name, ImVec2(-FLT_MIN, ImGui::GetTextLineHeight() * 2));

            if (ImGui::Button("Create file")) {
                currentFile = "";
                if (!file_name.empty()) {
                    currentFile.assign(file_name.begin(), file_name.end());
                }
                std::string filePath = currentDirectory.c_str() + currentFile;
                //std::cout << filePath << std::endl;
                SaveToFile(filePath.c_str(), "lol");

                ImVector<char> my_vector;
                if (my_vector.empty())
                    my_vector.push_back(0);

                AddIndexedImVector(next_tab_id, my_vector);

                // add new tab
                active_tabs.push_back(next_tab_id);
                tab_names.push_back(currentFile.c_str());
                next_tab_id++;

                file_name.clear();
            }

            if (ImGui::Button("Open file")) {
                currentFile = "";
                if (!file_name.empty()) {
                    currentFile.assign(file_name.begin(), file_name.end());
                }
                //my_str = OpenFile(currentFile.c_str());

                ImVector<char> my_vector;
                if (my_vector.empty())
                    my_vector.push_back(0);

                std::string filePath = currentDirectory.c_str() + currentFile;
                my_vector = OpenFile(filePath.c_str());

                AddIndexedImVector(next_tab_id, my_vector);

                // add new tab
                active_tabs.push_back(next_tab_id);
                tab_names.push_back(currentFile.c_str());
                next_tab_id++;

                file_name.clear();
            }

            ImGui::Text("Current Directory: %s", currentDirectory.c_str());
            if (ImGui::Button("Refresh")) {
                std::string command = "ls " + currentDirectory;
                displayedDir = RunConsoleCommand(command);
            }
            ImGui::Text("%s", displayedDir.c_str());

            ImGui::End();
        }

        {
            ImGui::Begin("Console");
            //ImGui::SetCursorPosY(ImGui::GetCursorPosY() + 10.0f);
            ImGui::SetCursorPosX(ImGui::GetCursorPosX() + 300.0f);
            ImGui::Image((void*)(intptr_t)my_image_texture2, ImVec2(150, 208));
            ImGui::SetCursorPosY(ImGui::GetCursorPosY() - 200.0f);
            
            static ImVector<char> custom_console_text;
            if (custom_console_text.empty())
                custom_console_text.push_back(0);
            MyInputText("##CustomConsoleText", &custom_console_text, ImVec2(-FLT_MIN, ImGui::GetTextLineHeight() * 2));

            if (ImGui::Button("Execute")) {
                std::string command = "";
                if (!custom_console_text.empty()) {
                    command.assign(custom_console_text.begin(), custom_console_text.end());
                }

                consoleOutputText = RunConsoleCommand(command);
            }

            // ImGui::Begin("Console", nullptr, ImGuiWindowFlags_NoResize);
            // ImVec2 windowSize(400, 300);
            // ImGui::SetWindowSize(windowSize);

            if (ImGui::Button("Compile (C++)")) {
                std::string filePath = currentDirectory.c_str() + currentFile;
                std::string command = "g++ -o " + FileNameWithoutDot(filePath.c_str()) + " " + filePath.c_str();
                //std::cout << command << std::endl;
                consoleOutputText = RunConsoleCommand(command);
            }

            if (ImGui::Button("Run (C++)")) {
                std::string filePath = currentDirectory.c_str() + currentFile;
                std::string command = "./" + FileNameWithoutDot(filePath.c_str());
                //std::cout << command << std::endl;
                consoleOutputText = RunConsoleCommand(command);
            }

            ImGui::Text("Output:");
            
            //ImDrawList* draw_list = ImGui::GetWindowDrawList();
            static float wrap_width = 300.0f;

            ImGui::PushTextWrapPos(ImGui::GetCursorPos().x + wrap_width);
            //ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.0f, 0.0f, 0.0f, 1.0f));
            ImGui::Text("%s", consoleOutputText.c_str());
            //ImGui::PopStyleColor();

            //draw_list->AddRectFilled(ImGui::GetItemRectMin(), ImGui::GetItemRectMax(), IM_COL32(0, 222, 255, 25));
            ImGui::PopTextWrapPos();
            

            ImGui::End();
        }

        /*
        {
            ImGui::Begin("OpenGL Texture Text");
            ImGui::Text("pointer = %x", my_image_texture);
            ImGui::Text("size = %d x %d", my_image_width, my_image_height);
            ImGui::Image((void*)(intptr_t)my_image_texture, ImVec2(my_image_width, my_image_height));
            ImGui::End();
        }*/

        
        // 1. Show the big demo window (Most of the sample code is in ImGui::ShowDemoWindow()! You can browse its code to learn more about Dear ImGui!).
        if (show_demo_window)
            ImGui::ShowDemoWindow(&show_demo_window);
        /*
        // 2. Show a simple window that we create ourselves. We use a Begin/End pair to create a named window.
        {
            static float f = 0.0f;
            static int counter = 0;

            ImGui::Begin("Hello, world!");                          // Create a window called "Hello, world!" and append into it.

            ImGui::Text("This is some useful text.");               // Display some text (you can use a format strings too)
            ImGui::Checkbox("Demo Window", &show_demo_window);      // Edit bools storing our window open/close state
            ImGui::Checkbox("Another Window", &show_another_window);

            ImGui::SliderFloat("float", &f, 0.0f, 1.0f);            // Edit 1 float using a slider from 0.0f to 1.0f
            ImGui::ColorEdit3("clear color", (float*)&clear_color); // Edit 3 floats representing a color

            if (ImGui::Button("Button"))                            // Buttons return true when clicked (most widgets return true when edited/activated)
                counter++;
            ImGui::SameLine();
            ImGui::Text("counter = %d", counter);

            ImGui::Text("Application average %.3f ms/frame (%.1f FPS)", 1000.0f / io.Framerate, io.Framerate);
            ImGui::End();
        }*/

        // 3. Show another simple window.
        if (show_another_window)
        {
            ImGui::Begin("Another Window", &show_another_window);   // Pass a pointer to our bool variable (the window will have a closing button that will clear the bool when clicked)
            ImGui::Text("Hello from another window!");
            if (ImGui::Button("Close Me"))
                show_another_window = false;
            ImGui::End();
        }

        // Rendering
        ImGui::Render();
        int display_w, display_h;
        glfwGetFramebufferSize(window, &display_w, &display_h);
        glViewport(0, 0, display_w, display_h);
        glClearColor(clear_color.x * clear_color.w, clear_color.y * clear_color.w, clear_color.z * clear_color.w, clear_color.w);
        glClear(GL_COLOR_BUFFER_BIT);
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

        glfwSwapBuffers(window);
    }
#ifdef __EMSCRIPTEN__
    EMSCRIPTEN_MAINLOOP_END;
#endif

    // Cleanup
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();

    glfwDestroyWindow(window);
    glfwTerminate();

    return 0;
}