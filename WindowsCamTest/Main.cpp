// WindowsCamTest.cpp : 定义应用程序的入口点。
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

//#define STB_IMAGE_IMPLEMENTATION
#include "stbimage/stb_image.h"
#include "stbimage/stb_image_resize.h"
#include <stdio.h>
#include <vector>
#include <string>
#include"ImGuiFileDialog-Lib_Only/ImGuiFileDialog.h"
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
IInterface* m_pShowUI = NULL;
int g_FlowState = 0;
static bool g_b_connect = false;
int g_functionType = 0;
int g_para1 = 0;
int g_para2 = 0;
int g_para3 = 0;

unsigned int g_camera_num = 0;
std::mutex camera_num_mutex;


std::string g_filepath = "E:\\ydc1";
std::string g_imagepath = "";

bool g_bSave = false;
bool g_bAsyncSave = false;
bool g_bPreview = false;
int g_max_count_per_second = 0;
int g_last_count_per_second = 0;
int g_captured_img = 0;
std::vector<int> img_per_second_list;
std::vector<int> cell_per_second_list;
std::vector<int> cell_total_list;
int cell_total;
int img_per_second_avg = 0;
int cell_per_second_avg = 0;
std::vector<int> second_list;
void SaveImageAsync(NativeCamera* inCamera) {
    int count = 0;
    std::string old_time_str;
    int count_in_second = 1;
    int cell_in_second = 1;
    struct tm timeInfo;
    time_t rawTime;
    int second = 0;
    while (true) {
        int cellNum = 0;
        if (g_bPreview) {
            if (g_bSave && std::filesystem::is_directory(g_filepath)) {
                time(&rawTime);
                localtime_s(&timeInfo, &rawTime);
                char buffer[100];
                strftime(buffer, 100, "Pic_%G_%m_%d_%H%M%S_blockId#", &timeInfo);
                if (std::string(buffer) != old_time_str) {
                    g_last_count_per_second = count_in_second;
                    if (count_in_second > g_max_count_per_second) {
                        g_max_count_per_second = count_in_second;
                    }
                    cell_total += cell_in_second;
                    img_per_second_list.push_back(count_in_second);
                    cell_per_second_list.push_back(cell_in_second);
                    cell_total_list.push_back(cell_total);
                    second_list.push_back(second++);
                    count_in_second = 1;
                    cell_in_second = 1;
                }
                old_time_str = std::string(buffer);
                char count_str[6];
                sprintf_s(count_str, 6, "%05d", count_in_second);
                std::string res = old_time_str + std::string(count_str) + ".bmp";
                std::filesystem::path imagePath = std::filesystem::path(g_filepath);
                imagePath /= std::filesystem::path(res);
                cellNum = inCamera->GetFrame(true, g_bAsyncSave, imagePath.string());
                if (cellNum > 0) {
                    count++;
                    g_captured_img = count;
                    count_in_second++;
                    cell_in_second += cellNum;
                }
            }
            else {
                inCamera->GetFrame(false, false, "");
            }
        }
    }

}
float average(const std::vector<int>& v) {
    float res = 0;
    for (const int& e : v) {
        res += e;
    }
    res /= v.size();
    return res;
}

LogWindows* g_LogWindow = new LogWindows();

void UpdateInstateCallback(int functionType, int para1, int para2, int para3)
{
    g_functionType = functionType;
    g_para1 = para1;
    g_para2 = para2;
    g_para3 = para3;
    if (functionType == 56) {
        g_b_connect = static_cast<bool>( para1);
    }

}

void TestInit()
{
    ::OutputDebugStringA("#22 TestInit.\n");
    //该方法不支持多线程调用
    //function<void(int, int, int, int)> call = std::bind(&UpdateInstateCallback, 
    //	std::tr1::placeholders::_1, std::tr1::placeholders::_2, 
    //	std::tr1::placeholders::_3, std::tr1::placeholders::_4);

    IInterface* pShowUI = IInterface::CreateInstance();
    m_pShowUI = pShowUI;

    pShowUI->SetInitStateCallback(&UpdateInstateCallback);//设置事件响应回调

    //初始化
    pShowUI->Init();
}

bool LoadTextureFromImage(cv::Mat image, GLuint* out_texture)
{
    glPixelStorei(GL_UNPACK_ALIGNMENT,(image.step &3) ? 1:4);
    glPixelStorei(GL_UNPACK_ROW_LENGTH,static_cast<GLint>(image.step/ image.elemSize()));
    GLenum internalformat = GL_RGB;
    if (image.channels() == 4) internalformat = GL_RGBA;
    if (image.channels() == 3) internalformat = GL_RGB;
    if (image.channels() == 2) internalformat = GL_RG;
    if(image.channels() == 1) internalformat = GL_RED;

    GLenum externalformat = GL_BGR;
    if(image.channels() == 1) externalformat = GL_RED;

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
        glTexImage2D(GL_TEXTURE_2D, 0, internalformat, image.cols, image.rows, 0, externalformat, GL_UNSIGNED_BYTE, image.ptr());
    }
    catch(std::exception & e) {
        std::cout << "exception : " << e.what() << std::endl;
    }

    glDeleteTextures(1, out_texture);
    *out_texture = image_texture;
    return true;
}
static void glfw_error_callback(int error, const char* description)
{
    fprintf(stderr, "Glfw Error %d: %s\n", error, description);
}

HANDLE g_hOupput = 0;
int APIENTRY wWinMain(_In_ HINSTANCE hInstance,
                     _In_opt_ HINSTANCE hPrevInstance,
                     _In_ LPWSTR    lpCmdLine,
                     _In_ int       nCmdShow)
{
    // Setup window
    TestInit();
    AllocConsole();
    HANDLE  g_hOutput = GetStdHandle(STD_OUTPUT_HANDLE);
    freopen("CONIN$", "r", stdin);
    freopen("CONOUT$", "w", stdout);
    freopen("CONOUT$", "w", stderr);
    glfwSetErrorCallback(glfw_error_callback);
    if (!glfwInit())
        return 1;

    // Decide GL+GLSL versions
    // GL 3.0 + GLSL 130
    const char* glsl_version = "#version 330";
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

    // Create window with graphics context
    int nScreenWidth = GetSystemMetrics(SM_CXSCREEN);
    int nScreenHeight = GetSystemMetrics(SM_CYSCREEN);

    GLFWwindow* window = glfwCreateWindow(nScreenWidth-300, nScreenHeight-200, "LSKJ-VS1000", NULL, NULL);
    if (window == NULL)
        return 1;
    glfwMakeContextCurrent(window);
    glfwSwapInterval(1); // Enable vsync
    // glad用于管理现代opengl 的函数指针
    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress))
    {
        std::cout << "Failed to initialize GLAD" << std::endl;
        return -1;
    }
    // Setup Dear ImGui context
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImPlot::CreateContext();
    ImGuiIO& io = ImGui::GetIO(); (void)io;
    //io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;     // Enable Keyboard Controls
    //io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;      // Enable Gamepad Controls

    // Setup Dear ImGui style
    ImGui::StyleColorsDark();
    //ImGui::StyleColorsClassic();

    // Setup Platform/Renderer backends
    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL3_Init(glsl_version);

    // main window state
    bool show_demo_window = false;
    bool b_show_capture_window = true;
    bool b_show_statis_window = true;

    // main menu state
    bool b_quit = false;

    // capture state
    bool b_start_capture = false;
    bool b_stop_capture = true;
    bool b_sample_in = true;
    int pos_selected = 39;
    const char* volume_items[] = { "10  uL","20  uL","30  uL","50  uL","100 uL" };
    int volume_array[5] = {10,20,30,50,100};
    int volume_current = 0;
    bool b_save = false;
    bool b_preview = true;
    enum Speed_Element { Element_1, Element_2, Element_3, Element_COUNT };
    int speed_array[3] = {10,20,30};
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

    enum Info_Item{Item_et,Item_ev,Item_ci,Item_cc,Item_tv,Item_s,Item_px,Item_py,Item_count};
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

    // preview state
    bool b_show_preview_window = true;

    // load image

    int my_image_width = 320;
    int my_image_offset_x = 40;
    int my_image_height = 480;
    int my_image_exposure_time = 170;
    int my_image_acquisition_frame_rate = 2000;
    GLuint my_image_texture = 0;
    //IM_ASSERT(ret);

    //log window
    g_LogWindow->AddLog("hello world \n");

    // Native camera
    NativeCamera* m_camera = new NativeCamera(g_LogWindow);
    if (m_camera->Init()) {
        m_camera->SetParas(my_image_width, my_image_height, my_image_offset_x, my_image_exposure_time, my_image_acquisition_frame_rate);
    }

    std::thread thread_save_img = std::thread(SaveImageAsync, m_camera);
    thread_save_img.detach();
    if (m_camera->SaveAsync()) {
        std::thread thread_save_image = std::thread([](NativeCamera* inCamera) {
            inCamera->SaveImageFromQueue();
            }, m_camera);
        thread_save_image.detach();
    }
    cv::Mat testImg;
    {
        testImg = cv::imread("C:\\0222\\1_\\Pic_2022_02_22_155135_blockId#58150.bmp");
    }

    // laser control

    std::string serialName = "\\\\.\\COM4";
    LaserControl* m_laser = new LaserControl(serialName.c_str(), laser_width, laser_frequency, laser_intensity, g_LogWindow);

    // mkdir g_filepath
    if (!std::filesystem::exists(std::filesystem::path(g_filepath))) {
        //std::filesystem::create_directories(std::filesystem::path(g_filepath));
        //g_LogWindow->AddLog("create path %s \n",g_filepath);
    }

    // Main loop
    while (!glfwWindowShouldClose(window))
    {
        if (b_quit) {
            break;
        }
        //cv::Mat* image_temp = nullptr;
        

        //ImPlot::ShowDemoWindow();

        if (b_save != g_bSave) {
            g_bSave = b_save;
        }
        if (b_preview && !g_bPreview) {
            m_camera->StartCapture();
            g_bPreview = b_preview;
        }
        if (!b_preview && g_bPreview) {
            m_camera->StopCapture();
            g_bPreview = b_preview;
        }

        cv::Mat image_temp = m_camera->OperateImageQueue(cv::Mat(), false);
        if (!image_temp.empty()) {
            bool bEmpty = image_temp.empty();
            cv::Mat channels[3];
            channels[0] = image_temp;
            channels[1] = image_temp;
            channels[2] = image_temp;
            cv::Mat img;
            cv::merge(channels,3,img);
            bool ret = LoadTextureFromImage(img,&my_image_texture);
            img.release();
            for (int i = 0; i < 3; i++) {
                channels[i].release();
            }
        }
        image_temp.release();

        //std::cout << "camera num" << m_camera->GetDeviceNum() << std::endl;
        std::get<1>(info_v[Item_tv]) = volume_array[volume_current];
        std::get<1>(info_v[Item_s]) = speed_array[speed_current_elem];
        std::get<1>(info_v[Item_py]) = int(pos_selected/8) + 1;
        std::get<1>(info_v[Item_px]) = pos_selected % 8 + 1;
        std::get<1>(info_v[Item_ci]) = g_captured_img;
        

        glfwPollEvents();

        // Start the Dear ImGui frame
        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();
        
        // main menu
        if (ImGui::BeginMainMenuBar()) {
            if (ImGui::BeginMenu("Option")) {

                ImGui::MenuItem("Quit", NULL, &b_quit);
                ImGui::EndMenu();
            }
            ImGui::EndMainMenuBar();
        }

        // main window
        {
            static float f = 0.0f;
            static int counter = 0;

            ImGui::Begin("Main Window");
            ImGuiIO& io = ImGui::GetIO();
            io.FontGlobalScale = 1.5;
            //const float MIN_SCALE = 0.3f;
            //const float MAX_SCALE = 2.0f;
            //ImGui::DragFloat("global scale", &io.FontGlobalScale, 0.005f, MIN_SCALE, MAX_SCALE, "%.2f", ImGuiSliderFlags_AlwaysClamp); // Scale everything
            ImGuiStyle& style = ImGui::GetStyle();
            style.FrameRounding = 4;
            if (ImGui::SliderFloat("FrameRounding", &style.FrameRounding, 0.0f, 12.0f, "%.0f"))
                style.GrabRounding = style.FrameRounding; // Make GrabRounding always the same value as FrameRounding
            style.SelectableTextAlign = ImVec2(0.5,0.5);
            ImGui::SliderFloat2("SelectableTextAlign", (float*)&style.SelectableTextAlign, 0.0f, 1.0f, "%.2f");
            ImGui::End();
        }
        // test window
        {
            ImGui::Begin("test window");
            if (ImGui::Button("clear")) {
                if (std::filesystem::exists(std::filesystem::path(g_filepath))) {
                    std::thread clear_thread = std::thread([]() {
                        g_LogWindow->AddLog("begin clear %s \n", g_filepath);
                        std::filesystem::remove_all(std::filesystem::path(g_filepath));
                        std::filesystem::create_directories(std::filesystem::path(g_filepath));
                        g_LogWindow->AddLog("end clear %s \n", g_filepath);
                        });
                    clear_thread.detach();
                }
            }
            ImGui::Text("Application average %.3f ms/frame (%.1f FPS)", 1000.0f / ImGui::GetIO().Framerate, ImGui::GetIO().Framerate);
            ImGui::End();

        }
        // capture window
        if (b_show_capture_window)
        {
            ImGui::Begin("Capture", &b_show_capture_window);
            
            // pos
            ImGui::Text("Choose pos :");
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
            ImGui::Separator();
            
            if (ImGui::Button("Start")) {
                m_pShowUI->CollectStart(std::get<1>(info_v[Item_tv]), std::get<1>(info_v[Item_s]), 0, std::get<1>(info_v[Item_py]), std::get<1>(info_v[Item_px]));
                //img_per_second_list.clear();
                //m_pShowUI->CollectStart(10, 10, 0, 5, 8);
            }
            // in and out
            if (ImGui::Button("Stop")) {
                m_pShowUI->StopCollect();
            }
            if (ImGui::Button("Box in out")) {
                m_pShowUI->OpenCloseDoor();
            }
            ImGui::Checkbox("Save Image", &b_save);
            // open Dialog Simple
            if (ImGui::Button("Open Path")) {
                ImGuiFileDialog::Instance()->OpenDialog("ChooseFileDlgKey", "Choose File", nullptr, ".");
            }
            /*if (ImGui::Button("Clear data")) {

                cell_per_second_list = std::vector<int>();
                cell_per_second_avg = 0;
                cell_total_list = std::vector<int>();
            }*/
            // display
            if (ImGuiFileDialog::Instance()->Display("ChooseFileDlgKey"))
            {
                if (ImGuiFileDialog::Instance()->IsOk())
                {
                    g_filepath = ImGuiFileDialog::Instance()->GetCurrentPath().c_str();
                }
                ImGuiFileDialog::Instance()->Close();
            }
            //save path
            ImGui::SameLine();
            ImGui::Text("%s", (char*)g_filepath.c_str());
            ImGui::End();
        }
        // debug window
        {
            ImGui::Begin("debug");


            // info table
            static ImGuiTableFlags flags = ImGuiTableFlags_Resizable | ImGuiTableFlags_Reorderable | ImGuiTableFlags_Hideable | ImGuiTableFlags_BordersOuter | ImGuiTableFlags_BordersV;

            if (ImGui::BeginTable("Info", 2, flags))
            {
                // Submit columns name with TableSetupColumn() and call TableHeadersRow() to create a row with a header in each column.
                // (Later we will show how TableSetupColumn() has other uses, optional flags, sizing weight etc.)
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
            ImGui::Text("Max Count:%d", (g_max_count_per_second));
            ImGui::Text("Last Count:%d", (g_last_count_per_second));
            ImGui::Text("functionType %d", g_functionType);
            ImGui::Text("para1 %d", g_para1);
            ImGui::Text("para2 %d", g_para2);
            ImGui::Text("para3 %d", g_para3);

            ImGui::End();
        }
        // properties window
        {
            ImGui::Begin("Properties");

            ImGui::Combo("Choose volume", &volume_current, volume_items, IM_ARRAYSIZE(volume_items));

            // speed slider
            const char* elems_names[Element_COUNT] = { "10 uL/min","20 uL/min","30 uL/min" };
            const char* elem_name = (speed_current_elem >= 0 && speed_current_elem < Element_COUNT) ? elems_names[speed_current_elem] : "Unknown";
            ImGui::SliderInt("Choose speed", &speed_current_elem, 0, Element_COUNT - 1, elem_name);

            // image setting
            if (ImGui::Button("Update Camera Setting")) {
                my_image_exposure_time = atoi(img_exposure_cstr);
                my_image_height = atoi(img_height_cstr);
                my_image_width = atoi(img_width_cstr);
                my_image_offset_x = atoi(img_offset_x_cstr);
                my_image_acquisition_frame_rate = atoi(img_acquisition_frame_rate_cstr);
                if (!g_bSave) {
                    if (m_camera->SetParas(my_image_width, my_image_height, my_image_offset_x, my_image_exposure_time, my_image_acquisition_frame_rate)) {
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
                delete m_laser;
                m_laser = new LaserControl(serialName.c_str(), laser_width, laser_frequency, laser_intensity,g_LogWindow);
            }
            ImGui::SameLine();
            if (ImGui::Button("Open")) {
                m_laser->OpenLaser();
                Sleep(10);
                m_laser->OpenLaser();
            }
            ImGui::SameLine();
            if (ImGui::Button("Close")) {
                m_laser->CloseLaser();
            }
            ImGui::SameLine();
            if (ImGui::Button("Update")) {
                laser_width = atoi(laser_width_cstr);
                laser_frequency = atoi(laser_frequency_cstr);
                laser_intensity = atoi(laser_intensity_cstr);
                m_laser->SetParas(laser_width, laser_frequency, laser_intensity);
            }
            ImGui::InputText("Frequency(Hz)", laser_frequency_cstr, IM_ARRAYSIZE(laser_frequency_cstr));
            ImGui::InputText("Width(ns)", laser_width_cstr, IM_ARRAYSIZE(laser_width_cstr));
            ImGui::InputText("current(mA)", laser_intensity_cstr, IM_ARRAYSIZE(laser_intensity_cstr));

            ImGui::End();

        }
        // preview window
        if (b_show_preview_window) {
            ImGui::Begin("Preview", &b_show_preview_window);   // Pass a pointer to our bool variable (the window will have a closing button that will clear the bool when clicked)
            ImGui::Checkbox("Preview", &b_preview);
            ImGui::Image((void*)(intptr_t)my_image_texture, ImVec2(my_image_width, my_image_height));
            ImGui::End();
        }
        // statis window
        if (b_show_statis_window)
        {
            ImGui::Begin("Statis", &b_show_statis_window);   // Pass a pointer to our bool variable (the window will have a closing button that will clear the bool when clicked)
            std::vector<float> plot_data;
            for (int i = 0; i < 1000; i++) {
                plot_data.push_back(i);
            }
            int bar_data[11] = {1,2,3,4,5,6,7,8,9,10,11};
            img_per_second_avg = average(img_per_second_list);
            std::vector<int> img_per_second_avg_list(second_list.size(),img_per_second_avg);
            //if (ImPlot::BeginPlot("My Plot")) {
            //    //ImPlot::PlotBars("img per second", img_per_second_list.data(), img_per_second_list.size());
            //    //ImPlot::PlotScatter("Data 1", bar_data, bar_data, 11);
            //    ImPlot::SetupAxes(NULL, NULL, ImPlotAxisFlags_AutoFit, ImPlotAxisFlags_AutoFit);

            //    ImPlot::PlotLine("img per second", second_list.data(), img_per_second_list.data(), second_list.size());
            //    ImPlot::PlotLine("avg", second_list.data(), img_per_second_avg_list.data(), second_list.size());
            //    ImPlot::EndPlot();
            //}
            cell_per_second_avg = average(cell_per_second_list);
            if (ImPlot::BeginPlot("Cell Per Second")) {
                //ImPlot::PlotBars("img per second", img_per_second_list.data(), img_per_second_list.size());
                //ImPlot::PlotScatter("Data 1", bar_data, bar_data, 11);
                ImPlot::SetupAxes(NULL, NULL, ImPlotAxisFlags_AutoFit, ImPlotAxisFlags_AutoFit);

                ImPlot::PlotLine("cell per second", second_list.data(), cell_per_second_list.data(), second_list.size());
                ImPlot::PlotLine("avg", second_list.data(), img_per_second_avg_list.data(), second_list.size());
                ImPlot::EndPlot();
            }
            if (ImPlot::BeginPlot("Cell Total")) {
                //ImPlot::PlotBars("img per second", img_per_second_list.data(), img_per_second_list.size());
                //ImPlot::PlotScatter("Data 1", bar_data, bar_data, 11);
                ImPlot::SetupAxes(NULL, NULL, ImPlotAxisFlags_AutoFit, ImPlotAxisFlags_AutoFit);
                ImPlot::PlotLine("img per second", second_list.data(), cell_total_list.data(), second_list.size());
                ImPlot::EndPlot();
            }
            ImGui::End();
        }
        g_LogWindow->Draw("Log");

        // Rendering
        ImGui::Render();
        int display_w, display_h;
        glfwGetFramebufferSize(window, &display_w, &display_h);
        glViewport(0, 0, display_w, display_h);
        glClearColor(0.f, 0.f, 0.f, 1.0f);//设置清理的颜色，即背景颜色
        //glClearColor(clear_color.x * clear_color.w, clear_color.y * clear_color.w, clear_color.z * clear_color.w, clear_color.w);
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

    delete m_laser;

    return 0;
}


