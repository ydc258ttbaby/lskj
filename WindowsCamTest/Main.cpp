﻿// WindowsCamTest.cpp : 定义应用程序的入口点。
//
#include "framework.h"
#include "Main.h"
#include "FlowCam\IInterface.h"

#include <opencv2/opencv.hpp>
#include "NativeCamera.h"
#include "LaserControl.h"
#include <iostream>
#include <glad/glad.h>
#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl3.h"
#include <GLFW/glfw3.h> // Will drag system OpenGL headers
#include "LogWindows.h"
#include "implot-master/implot.h"
#include "CellDetect.h"
#include"ImGuiFileDialog-Lib_Only/ImGuiFileDialog.h"

//#define STB_IMAGE_IMPLEMENTATION
#include "stbimage/stb_image.h"
#include "stbimage/stb_image_resize.h"
#include <stdio.h>
#include <vector>
#include <string>
#include <Windows.h>
#include <unordered_map>
#include <tuple>
#include <thread>
#include <mutex>
#include <shared_mutex>
#include <chrono>
#include <filesystem>
#include <algorithm>
#if defined(IMGUI_IMPL_OPENGL_ES2)
#include <GLES2/gl2.h>
#endif

// [Win32] Our example includes a copy of glfw3.lib pre-compiled with VS2010 to maximize ease of testing and compatibility with old VS compilers.
// To link with VS2010-era libraries, VS2015+ requires linking with legacy_stdio_definitions.lib, which we do using this pragma.
// Your own project should not be affected, as you are likely to link with a newer binary of GLFW that is adequate for your version of Visual Studio.
#if defined(_MSC_VER) && (_MSC_VER >= 1900) && !defined(IMGUI_DISABLE_WIN32_FUNCTIONS)
#pragma comment(lib, "legacy_stdio_definitions")
#endif

// global flow
IInterface* g_flow = NULL;
int g_function_type = 0;
int g_para1 = 0;
int g_para2 = 0;
int g_para3 = 0;

// global image save 
std::string g_file_path = "E:\\ydc";
bool g_b_save = false;
bool g_b_async_save = false;
bool g_b_preview = false;

// global plot
std::vector<int> g_second_array;
std::vector<int> g_img_per_second_array;
std::vector<int> g_cell_per_second_array;
std::vector<int> g_cell_total_array;
int g_cell_total;
int g_cell_per_second_avg = 0;
int g_img_per_second_avg = 0;

// global logger
LogWindows* g_LogWindow = new LogWindows();

// detect image path
const int g_detect_class_num = 3; 
const int g_detect_preview_num = 10;
const int g_detect_img_width = 100;
const int g_detect_img_height = 100;
std::vector<std::vector<std::string> > g_detect_img_paths(g_detect_class_num,std::vector<std::string>(g_detect_preview_num,""));
std::vector<std::vector<GLuint> > g_detect_tex_ids(g_detect_class_num, std::vector<GLuint>(g_detect_preview_num, 0));

//progress
float g_progress = 0;

void SaveImageAsync(NativeCamera* in_camera) {
    int count = 0;
    int second = 0;
    int img_per_second = 1;
    int cell_per_second = 1;
    std::string old_time_str;
    struct tm time_info;
    time_t raw_time;
    while (true) {
        int cell_num = 0;
        if (g_b_preview) {
            if (g_b_save && std::filesystem::is_directory(g_file_path)) {
                time(&raw_time);
                localtime_s(&time_info, &raw_time);
                char time_info_buffer[100];
                strftime(time_info_buffer, 100, "Pic_%G_%m_%d_%H%M%S_blockId#", &time_info);
                if (std::string(time_info_buffer) != old_time_str) {
                    g_cell_total += cell_per_second;
                    g_img_per_second_array.push_back(img_per_second);
                    g_cell_per_second_array.push_back(cell_per_second);
                    g_cell_total_array.push_back(g_cell_total);
                    g_second_array.push_back(second++);
                    img_per_second = 1;
                    cell_per_second = 1;
                }
                old_time_str = std::string(time_info_buffer);
                char count_str[6];
                sprintf_s(count_str, 6, "%05d", img_per_second);
                std::string image_save_name = old_time_str + std::string(count_str) + ".bmp";
                std::filesystem::path image_save_path = std::filesystem::path(g_file_path);
                image_save_path /= std::filesystem::path(image_save_name);
                cell_num = in_camera->GetFrame(true, g_b_async_save, image_save_path.string());
                if (cell_num > 0) {
                    count++;
                    img_per_second++;
                    cell_per_second += cell_num;
                }
            }
            else {
                in_camera->GetFrame(false, false, "");
            }
        }
    }

}
float VectorAverage(const std::vector<int>& v) {
    float avg = 0;
    for (const int& e : v) {
        avg += e;
    }
    avg /= v.size();
    return avg;
}

void UpdateInstateCallback(int in_function_type, int in_para1, int in_para2, int in_para3)
{
    g_function_type = in_function_type;
    g_para1 = in_para1;
    g_para2 = in_para2;
    g_para3 = in_para3;

}

void FlowInit()
{
    ::OutputDebugStringA("#22 FlowInit.\n");

    IInterface* pShowUI = IInterface::CreateInstance();
    g_flow = pShowUI;

    pShowUI->SetInitStateCallback(&UpdateInstateCallback);//设置事件响应回调
    pShowUI->Init();
}

bool LoadTextureFromImage(cv::Mat in_image, GLuint* out_texture)
{
    glPixelStorei(GL_UNPACK_ALIGNMENT, (in_image.step & 3) ? 1 : 4);
    glPixelStorei(GL_UNPACK_ROW_LENGTH, static_cast<GLint>(in_image.step / in_image.elemSize()));
    GLenum internal_format = GL_RGB;
    if (in_image.channels() == 4) internal_format = GL_RGBA;
    if (in_image.channels() == 3) internal_format = GL_RGB;
    if (in_image.channels() == 2) internal_format = GL_RG;
    if (in_image.channels() == 1) internal_format = GL_RED;

    GLenum external_format = GL_BGR;
    if (in_image.channels() == 1) external_format = GL_RED;

    GLuint image_texture;
    glGenTextures(1, &image_texture);
    glBindTexture(GL_TEXTURE_2D, image_texture);

    // Setup filtering parameters for display
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE); // This is required on WebGL for non power-of-two textures
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE); // Same

    // Upload pixels into texture
    try {
        glTexImage2D(GL_TEXTURE_2D, 0, internal_format, in_image.cols, in_image.rows, 0, external_format, GL_UNSIGNED_BYTE, in_image.ptr());
    }
    catch (std::exception& e) {
        std::cout << "exception : " << e.what() << std::endl;
    }

    glDeleteTextures(1, out_texture);
    *out_texture = image_texture;
    return true;
}
static void GlfwErrorCallback(int in_error, const char* in_description)
{
    fprintf(stderr, "Glfw Error %d: %s\n", in_error, in_description);
}
cv::Mat GrayToRGB(cv::Mat in_gray_image) {
    cv::Mat channels[3];
    channels[0] = in_gray_image;
    channels[1] = in_gray_image;
    channels[2] = in_gray_image;
    cv::Mat img;
    cv::merge(channels, 3, img);
    return img;
}
int APIENTRY wWinMain(_In_ HINSTANCE hInstance,
    _In_opt_ HINSTANCE hPrevInstance,
    _In_ LPWSTR    lpCmdLine,
    _In_ int       nCmdShow)
{
    // Flow init
    FlowInit();

    // open console
    AllocConsole();
    HANDLE  g_hOutput = GetStdHandle(STD_OUTPUT_HANDLE);
    freopen("CONIN$", "r", stdin);
    freopen("CONOUT$", "w", stdout);
    freopen("CONOUT$", "w", stderr);

    // glfw init
    glfwSetErrorCallback(GlfwErrorCallback);
    if (!glfwInit())
        return 1;

    // opengl version
    const char* glsl_version = "#version 330";
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

    // create window
    int nScreenWidth = GetSystemMetrics(SM_CXSCREEN);
    int nScreenHeight = GetSystemMetrics(SM_CYSCREEN);
    GLFWwindow* window = glfwCreateWindow(nScreenWidth, nScreenHeight, "LSKJ-VS1000", NULL, NULL);
    if (window == NULL)
        return 1;
    glfwMakeContextCurrent(window);
    glfwSwapInterval(1); 
    // glad bind to window
    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress))
    {
        std::cout << "Failed to initialize GLAD" << std::endl;
        return -1;
    }
    // Setup ImGui context
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImPlot::CreateContext();
    ImGuiIO& io = ImGui::GetIO(); (void)io;
    //io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;     // Enable Keyboard Controls
    //io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;      // Enable Gamepad Controls

    // Setup Dear ImGui style
    ImGui::StyleColorsDark();

    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL3_Init(glsl_version);

    // capture state
    bool b_start_capture = false;
    bool b_stop_capture = true;
    int pos_selected = 39;
    const char* volume_items[] = { "10  uL","20  uL","30  uL","50  uL","100 uL" };
    int volume_array[5] = { 10,20,30,50,100 };
    int volume_current = 0;
    bool b_save = false;
    bool b_preview = false;
    enum Speed_Element { Element_1, Element_2, Element_3, Element_COUNT };
    int speed_array[3] = { 10,20,30 };
    int speed_current_elem = Element_1;
    char img_width_cstr[128] = "320";
    char img_offset_x_cstr[128] = "40";
    char img_height_cstr[128] = "480";
    char img_exposure_cstr[128] = "170";
    char img_acquisition_frame_rate_cstr[128] = "2000";

    char laser_frequency_cstr[128] = "5000";
    char laser_width_cstr[128] = "200";
    char laser_intensity_cstr[128] = "100";
    int laser_frequency = atoi(laser_frequency_cstr);
    int laser_width = atoi(laser_width_cstr);
    int laser_intensity = atoi(laser_intensity_cstr);
    ImVec4 clear_color = ImVec4(0.45f, 0.55f, 0.60f, 1.00f);

    enum Info_Item { Item_et, Item_ev, Item_ci, Item_cc, Item_tv, Item_s, Item_px, Item_py, Item_count };
    std::vector<std::tuple<std::string, float, std::string> > info_v = {
    std::make_tuple("elapsed time",0.0,"s"),
    std::make_tuple("elapsed volume",0.0,"uL"),
    std::make_tuple("captured images",0.0,""),
    std::make_tuple("captured cells",0.0,""),
    std::make_tuple("total volume",0.0,"uL"),
    std::make_tuple("speed",0.0,"uL/min"),
    std::make_tuple("pos x",0.0,""),
    std::make_tuple("pos y",0.0,""),

    };

    // load image
    GLuint tex_id_preview = 0;
    // log window
    g_LogWindow->AddLog("hello world \n");

    // Native camera
    NativeCamera* camera = new NativeCamera(g_LogWindow, g_detect_class_num,g_detect_preview_num);
    if (camera->Init()) {
        camera->SetParas(atoi(img_width_cstr), atoi(img_height_cstr), atoi(img_offset_x_cstr), atoi(img_exposure_cstr), atoi(img_acquisition_frame_rate_cstr));
    }

    std::thread thread_save_img = std::thread(SaveImageAsync, camera);
    thread_save_img.detach();
    if (camera->SaveAsync()) {
        std::thread thread_save_image = std::thread([](NativeCamera* in_camera) {
            in_camera->SaveImageFromQueue();
            }, camera);
        thread_save_image.detach();
    }

    // laser control
    std::string serial_name = "\\\\.\\COM4";
    LaserControl* laser = new LaserControl(serial_name.c_str(), laser_width, laser_frequency, laser_intensity, g_LogWindow);

    // Main loop
    while (!glfwWindowShouldClose(window))
    {
        if (b_save != g_b_save) {
            g_b_save = b_save;
        }
        if (b_preview && !g_b_preview) {
            camera->StartCapture();
            g_b_preview = b_preview;
        }
        if (!b_preview && g_b_preview) {
            camera->StopCapture();
            g_b_preview = b_preview;
        }

        // load preview image
        cv::Mat image_temp = camera->OperateImageQueue(cv::Mat(), false);
        if (!image_temp.empty()) {
            bool ret = LoadTextureFromImage(GrayToRGB(image_temp), &tex_id_preview);
        }
        image_temp.release();
        // load detect image 
        for (int i = 0; i < g_detect_class_num; i++) {
            std::vector<cv::Mat> temp_images = camera->OperateDetectImageQueue(cv::Mat(),false,i);
            for (int j = 0; j < temp_images.size(); j++) {
                cv::Mat detect_img = GrayToRGB(temp_images[j]);
                if (!detect_img.empty()) {
                    LoadTextureFromImage(detect_img, &(g_detect_tex_ids[i][j]));
                }
            }
        }

        //std::cout << "camera num" << camera->GetDeviceNum() << std::endl;
        std::get<1>(info_v[Item_tv]) = volume_array[volume_current];
        std::get<1>(info_v[Item_s]) = speed_array[speed_current_elem];
        std::get<1>(info_v[Item_py]) = int(pos_selected / 8) + 1;
        std::get<1>(info_v[Item_px]) = pos_selected % 8 + 1;
        std::get<1>(info_v[Item_ci]) = 0;


        glfwPollEvents();

        // style init
        ImGuiIO& io = ImGui::GetIO();
        io.FontGlobalScale = 1.5;
        ImGuiStyle& style = ImGui::GetStyle();
        style.FrameRounding = 4;
        //style.SelectableTextAlign = ImVec2(0.5, 0.5);

        ImGuiWindowFlags window_flags = 0;
        window_flags |= ImGuiWindowFlags_NoTitleBar;
        window_flags |= ImGuiWindowFlags_NoScrollbar;
        //window_flags |= ImGuiWindowFlags_MenuBar;
        //window_flags |= ImGuiWindowFlags_NoMove;
        //window_flags |= ImGuiWindowFlags_NoResize;
        //window_flags |= ImGuiWindowFlags_NoCollapse;
        //window_flags |= ImGuiWindowFlags_NoNav;
        //window_flags |= ImGuiWindowFlags_NoBackground;
        //window_flags |= ImGuiWindowFlags_NoBringToFrontOnFocus;
        //window_flags |= ImGuiWindowFlags_UnsavedDocument;

        // Start the Dear ImGui frame
        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        // main menu
        if (ImGui::BeginMainMenuBar()) {
            if (ImGui::BeginMenu("Option")) {
                ImGui::EndMenu();
            }
            ImGui::EndMainMenuBar();
        }
        // capture window
        {
            ImGui::Begin("Capture", NULL, window_flags);
            ImGui::PushStyleVar(ImGuiStyleVar_SelectableTextAlign,ImVec2(0.5,0.5));

            // pos
            for (int r = 0; r < 5; r++) {
                for (int c = 0; c < 8; c++) {
                    if (c > 0)
                        ImGui::SameLine();
                    ImGui::PushID(r * 8 + c);
                    std::string AtoE[5] = { "A","B","C","D","E" };
                    std::string select_text = AtoE[r] + std::to_string(c + 1);
                    if (ImGui::Selectable(select_text.c_str(), pos_selected == (r * 8 + c), 0, ImVec2(40, 40))) {
                        pos_selected = r * 8 + c;
                    }
                    ImGui::PopID();
                }
            }
            ImGui::PopStyleVar(1);

            ImGui::Separator();
            //ImGui::PushItemWidth(-FLT_MIN);

            int current_region_width = ImGui::GetContentRegionAvail().x;
            //ImGui::BeginGroup();
            {
                if (ImGui::Button("Start", ImVec2(current_region_width * 0.5f, 40))) {
                    g_flow->CollectStart(std::get<1>(info_v[Item_tv]), std::get<1>(info_v[Item_s]), 0, std::get<1>(info_v[Item_py]), std::get<1>(info_v[Item_px]));
                }
                if (ImGui::IsItemHovered()) {
                    char tip_buffer[100];
                    sprintf(tip_buffer,"volume: %.0f \nspeed : %.0f \npos   : %.0f %.0f \n", std::get<1>(info_v[Item_tv]), std::get<1>(info_v[Item_s]), std::get<1>(info_v[Item_py]), std::get<1>(info_v[Item_px]));
                    ImGui::SetTooltip(tip_buffer);
                }
                //ImGui::EndGroup();
            }
            ImGui::SameLine();
            if (ImGui::Button("Stop", ImVec2(current_region_width * 0.5f,40))) {
                g_flow->StopCollect();
            }
            ImGui::PushStyleColor(ImGuiCol_PlotHistogram, ImVec4(1.0,0.43,0.35,1.0));
            ImGui::ProgressBar(g_progress, ImVec2(current_region_width * 1.0f, 4));
            ImGui::PopStyleColor(1);

            if (ImGui::Button("Box in/out", ImVec2(current_region_width * 0.5f, 40))) {
                g_flow->OpenCloseDoor();
            }
            ImGui::SameLine();
            // open path
            if (ImGui::Button("Open Path", ImVec2(current_region_width * 0.5f, 40))) {
                ImGuiFileDialog::Instance()->OpenDialog("ChooseFileDlgKey", "Choose File", nullptr, ".");
            }
            if (ImGuiFileDialog::Instance()->Display("ChooseFileDlgKey"))
            {
                if (ImGuiFileDialog::Instance()->IsOk())
                {
                    g_file_path = ImGuiFileDialog::Instance()->GetCurrentPath().c_str();
                }
                ImGuiFileDialog::Instance()->Close();
            }
            //save path
            ImGui::Checkbox("Save image in: ", &b_save);
            ImGui::SameLine();
            if (std::filesystem::is_directory(std::filesystem::path(g_file_path))) {
                ImGui::Text(g_file_path.c_str());
            }
            else {
                ImGui::TextColored(ImVec4(1.0,0.0,0.0,1.0), g_file_path.c_str());
                if (ImGui::IsItemHovered()) {
                    ImGui::SetTooltip("Path not exists");
                }
            }
            ImGui::Text("%.3f ms/frame (%.1f FPS)", 1000.0f / ImGui::GetIO().Framerate, ImGui::GetIO().Framerate);
            ImGui::End();
        }
        // preview window
        {
            ImGui::Begin("Preview", NULL, window_flags);   // Pass a pointer to our bool variable (the window will have a closing button that will clear the bool when clicked)
            ImGui::Image((void*)(intptr_t)tex_id_preview, ImVec2(atoi(img_width_cstr), atoi(img_height_cstr)));
            ImGui::Checkbox("Preview", &b_preview);
            ImGui::End();
        }
        // statis window
        {
            ImGui::Begin("Statis", NULL, window_flags);   // Pass a pointer to our bool variable (the window will have a closing button that will clear the bool when clicked)
            // plot data prepare
            g_cell_per_second_avg = VectorAverage(g_cell_per_second_array);
            g_img_per_second_avg = VectorAverage(g_img_per_second_array);
            std::vector<int> cell_per_second_avg_list(g_second_array.size(), g_cell_per_second_avg);
            std::vector<int> img_per_second_avg_list(g_second_array.size(), g_img_per_second_avg);
            // begin plot
            static ImPlotSubplotFlags flags = ImPlotSubplotFlags_NoTitle;
            static float rratios[] = { 1,1 };
            static float cratios[] = { 1,1,1 };
            if (ImPlot::BeginSubplots("My Subplots", 2, 2, ImVec2(-1, -1), flags, rratios, cratios)) {
                if (ImPlot::BeginPlot("")) {
                    ImPlot::SetupAxes("time(s)", NULL, ImPlotAxisFlags_AutoFit, ImPlotAxisFlags_AutoFit);
                    ImPlot::SetupLegend(ImPlotLocation_SouthEast, ImPlotLegendFlags_None);
                    ImPlot::PlotLine("cell per second", g_second_array.data(), g_cell_per_second_array.data(), g_second_array.size());
                    ImPlot::PlotLine("avg", g_second_array.data(), cell_per_second_avg_list.data(), g_second_array.size());
                    ImPlot::EndPlot();
                }
                if (ImPlot::BeginPlot("")) {
                    ImPlot::SetupAxes("time(s)", NULL, ImPlotAxisFlags_AutoFit, ImPlotAxisFlags_AutoFit);
                    ImPlot::SetupLegend(ImPlotLocation_SouthEast, ImPlotLegendFlags_None);
                    ImPlot::PlotLine("cell total", g_second_array.data(), g_cell_total_array.data(), g_second_array.size());
                    ImPlot::EndPlot();
                }
                if (ImPlot::BeginPlot("")) {
                    ImPlot::SetupAxes("time(s)", NULL, ImPlotAxisFlags_AutoFit, ImPlotAxisFlags_AutoFit);
                    ImPlot::SetupLegend(ImPlotLocation_SouthEast, ImPlotLegendFlags_None);
                    ImPlot::PlotLine("img per second", g_second_array.data(), g_img_per_second_array.data(), g_second_array.size());
                    ImPlot::PlotLine("avg", g_second_array.data(), img_per_second_avg_list.data(), g_second_array.size());
                    ImPlot::EndPlot();
                }
                ImPlot::EndSubplots();
            }
            ImGui::End();
        }
        // detect window
        {
            ImGui::Begin("detect", NULL, window_flags);
            ImGui::BeginChild("scrolling", ImVec2(0, 0), false, ImGuiWindowFlags_HorizontalScrollbar);
            for (int i = 0; i < g_detect_class_num; i++) {
                std::string btn_text = "class " + std::to_string(i);
                if (ImGui::Button(btn_text.c_str(),ImVec2(100,100))) {

                }
                ImGui::SameLine();
                for (int j = 0; j < g_detect_preview_num; j++) {
                    ImGui::Image((void*)(intptr_t)g_detect_tex_ids[i][j], ImVec2(g_detect_img_width, g_detect_img_height));
                    ImGui::SameLine();
                }
                ImGui::NewLine();
            }
            ImGui::EndChild();
            ImGui::End();
        }
        // debug window
        {
            ImGui::Begin("debug", NULL, window_flags);
            ImGuiTabBarFlags tab_bar_flags = ImGuiTabBarFlags_None;
            if (ImGui::BeginTabBar("MyTabBar", tab_bar_flags)) {
                // info tab
                if (ImGui::BeginTabItem("Info"))
                {
                    static ImGuiTableFlags flags = ImGuiTableFlags_Resizable | ImGuiTableFlags_Reorderable | ImGuiTableFlags_Hideable | ImGuiTableFlags_BordersOuter | ImGuiTableFlags_BordersV;
                    if (ImGui::BeginTable("Info", 2, flags))
                    {
                        ImGui::TableSetupColumn("Info");
                        ImGui::TableSetupColumn("Value");
                        ImGui::TableHeadersRow();
                        for (int row = 0; row < Item_count; row++)
                        {
                            ImGui::TableNextRow();
                            ImGui::TableSetColumnIndex(0);
                            ImGui::Text(std::get<0>(info_v[row]).c_str(), 0, row);
                            ImGui::TableSetColumnIndex(1);
                            ImGui::Text((std::to_string(static_cast<int>(std::get<1>(info_v[row]))) + " " + std::get<2>(info_v[row])).c_str(), 1, row);
                        }
                        ImGui::EndTable();
                    }
                    ImGui::Text("functionType %d", g_function_type);
                    ImGui::Text("para1 %d", g_para1);
                    ImGui::Text("para2 %d", g_para2);
                    ImGui::Text("para3 %d", g_para3);
                    ImGui::EndTabItem();
                }
                // properties tab
                if (ImGui::BeginTabItem("Properties")) {
                    ImGui::Combo("Choose volume", &volume_current, volume_items, IM_ARRAYSIZE(volume_items));
                    // speed slider
                    const char* elems_names[Element_COUNT] = { "10 uL/min","20 uL/min","30 uL/min" };
                    const char* elem_name = (speed_current_elem >= 0 && speed_current_elem < Element_COUNT) ? elems_names[speed_current_elem] : "Unknown";
                    ImGui::SliderInt("Choose speed", &speed_current_elem, 0, Element_COUNT - 1, elem_name);
                    // image setting
                    if (ImGui::Button("Update Camera Setting")) {
                        if (!g_b_save) {
                            if (camera->SetParas(atoi(img_width_cstr), atoi(img_height_cstr), atoi(img_offset_x_cstr), atoi(img_exposure_cstr), atoi(img_acquisition_frame_rate_cstr))) {
                                std::cout << "set paras success " << std::endl;
                            }
                            else {
                                std::cout << "set paras failed " << std::endl;
                            }
                        }
                    }
                    ImGui::InputText("Width", img_width_cstr, IM_ARRAYSIZE(img_width_cstr));
                    ImGui::InputText("Height", img_height_cstr, IM_ARRAYSIZE(img_height_cstr));
                    ImGui::InputText("OffsetX", img_offset_x_cstr, IM_ARRAYSIZE(img_offset_x_cstr));
                    ImGui::InputText("Exposure(us)", img_exposure_cstr, IM_ARRAYSIZE(img_exposure_cstr));
                    ImGui::InputText("FrameRate", img_acquisition_frame_rate_cstr, IM_ARRAYSIZE(img_acquisition_frame_rate_cstr));
                    // laser setting
                    ImGui::Separator();
                    ImGui::Text("Laser");
                    if (ImGui::Button("Reconnect")) {
                        delete laser;
                        laser = new LaserControl(serial_name.c_str(), laser_width, laser_frequency, laser_intensity, g_LogWindow);
                    }
                    ImGui::SameLine();
                    if (ImGui::Button("Open")) {
                        laser->OpenLaser();
                        Sleep(10);
                        laser->OpenLaser();
                    }
                    ImGui::SameLine();
                    if (ImGui::Button("Close")) {
                        laser->CloseLaser();
                    }
                    ImGui::SameLine();
                    if (ImGui::Button("Update")) {
                        laser_width = atoi(laser_width_cstr);
                        laser_frequency = atoi(laser_frequency_cstr);
                        laser_intensity = atoi(laser_intensity_cstr);
                        laser->SetParas(laser_width, laser_frequency, laser_intensity);
                    }
                    ImGui::InputText("Frequency(Hz)", laser_frequency_cstr, IM_ARRAYSIZE(laser_frequency_cstr));
                    ImGui::InputText("Width(ns)", laser_width_cstr, IM_ARRAYSIZE(laser_width_cstr));
                    ImGui::InputText("current(mA)", laser_intensity_cstr, IM_ARRAYSIZE(laser_intensity_cstr));
                    ImGui::EndTabItem();
                }
                // test tab
                if (ImGui::BeginTabItem("test")) {
                    if (ImGui::Button("clear")) {
                        if (std::filesystem::exists(std::filesystem::path(g_file_path))) {
                            std::thread clear_thread = std::thread([]() {
                                g_LogWindow->AddLog("begin clear %s \n", g_file_path);
                                std::filesystem::remove_all(std::filesystem::path(g_file_path));
                                std::filesystem::create_directories(std::filesystem::path(g_file_path));
                                g_LogWindow->AddLog("end clear %s \n", g_file_path);
                                });
                            clear_thread.detach();
                        }
                    }
                    ImGui::EndTabItem();
                }
                ImGui::EndTabBar();
            }
            ImGui::End();
        }
        // Log window
        {
            g_LogWindow->Draw("Log", NULL, window_flags);
        }
        
        ImGui::Render();
        int display_w, display_h;
        glfwGetFramebufferSize(window, &display_w, &display_h);
        glViewport(0, 0, display_w, display_h);
        glClearColor(0.f, 0.f, 0.f, 1.0f);//设置清理的颜色，即背景颜色
        glClear(GL_COLOR_BUFFER_BIT);
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
        glfwSwapBuffers(window);
    }

    // Cleanup
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();
    ImPlot::DestroyContext();

    glfwDestroyWindow(window);
    glfwTerminate();

    delete laser;

    return 0;
}


