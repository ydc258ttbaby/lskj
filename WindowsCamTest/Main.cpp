// WindowsCamTest.cpp : 定义应用程序的入口点。
//
#define NO_WIN32_LEAN_AND_MEAN
#include "framework.h"
#include "Main.h"
#include "FlowCam\IInterface.h"
#include <Shlobj.h>
#include <opencv2/opencv.hpp>
#include "NativeCamera.h"
#include "LaserControl.h"
#include <iostream>
#include <glad/glad.h>
#include <tchar.h>
#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl3.h"
#include <GLFW/glfw3.h> // Will drag system OpenGL headers
#include "LogWindows.h"
#include "implot-master/implot.h"
#include "CellDetect.h"
#include "ImGuiFileDialog-Lib_Only/ImGuiFileDialog.h"
#include "Condition.h"
//setlocale(LC_ALL, "zh-CN");


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
std::string g_file_path = "E:\\Data\\CameraImages\\20220330";
bool g_b_save = false;
bool g_b_async_save = false;
bool g_b_preview = false;

// global plot
std::vector<float> g_second_array;
std::vector<float> g_img_per_second_array;
std::vector<float> g_cell_per_second_array;
std::vector<float> g_cell_per_img_array;
std::vector<float> g_cell_total_array;
std::vector<float> g_cell_avg_3s_array;
std::vector<float> g_cell_avg_history_array;
std::vector<float> g_img_avg_history_array;
std::vector<float> g_cell_area_array;
std::vector<float> g_cell_peri_array;
std::vector<float> g_cell_dia_array;
std::vector<float> g_cell_short_array;
std::vector<float> g_cell_long_array;
std::vector<float> g_cell_vol_array;
std::vector<float> g_cell_ell_array;
std::vector<float> g_cell_round_array;

int g_cell_total;
int g_img_total;
float g_cell_per_second_avg = 0;
float g_img_per_second_avg = 0;
int g_second = 0;
int g_img_per_second = 0;
int g_cell_per_second = 0;
// global logger
LogWindows* g_LogWindow = new LogWindows();

// detect image path
const int g_detect_class_num = 3;
const int g_detect_preview_num = 8;
const int g_detect_img_width = 100;
const int g_detect_img_height = 100;
std::vector<std::vector<std::string> > g_detect_img_paths(g_detect_class_num, std::vector<std::string>(g_detect_preview_num, ""));
std::vector<std::vector<GLuint> > g_detect_tex_ids(g_detect_class_num, std::vector<GLuint>(g_detect_preview_num, 0));

//progress
float g_progress = 0;

// start time
std::chrono::steady_clock::time_point g_time_start = std::chrono::steady_clock::now();

// mutex
std::mutex g_img_plot_mutex;

//set filter index
bool set_dia = false;
bool set_peri = false;
bool set_area = false;
bool set_vol = false;
bool set_ell = false;

int g_set_filter_store = 0;
std::string g_img_filter_path = "";

int g_cell_filter = 0;

std::vector<std::string> g_condition_names = {u8"直径",u8"面积",u8"周长",u8"体积",u8"偏心率",u8"圆度" };
int g_property_int = 0;
int g_logic = 0;
std::vector<CellInfo> g_filter_cellinfos;
std::vector<CellInfo> g_show_cellinfos;

std::vector<Condition> g_conditions;

double RandomGauss() {
    static double V1, V2, S;
    static int phase = 0;
    double X;
    if (phase == 0) {
        do {
            double U1 = (double)rand() / RAND_MAX;
            double U2 = (double)rand() / RAND_MAX;
            V1 = 2 * U1 - 1;
            V2 = 2 * U2 - 1;
            S = V1 * V1 + V2 * V2;
        } while (S >= 1 || S == 0);

        X = V1 * sqrt(-2 * log(S) / S);
    }
    else
        X = V2 * sqrt(-2 * log(S) / S);
    phase = 1 - phase;
    return X;
}

template <int N>
struct NormalDistribution {
    NormalDistribution(double mean, double sd) {
        for (int i = 0; i < N; ++i)
            Data[i] = RandomGauss() * sd + mean;
    }
    double Data[N];
};

template <typename T>
void ClearVector(std::vector<T>& inVector) {
    std::vector<T> init_vector;
    inVector.swap(init_vector);
}

void ClearPlot() {
    ClearVector(g_second_array);
    ClearVector(g_img_per_second_array);
    ClearVector(g_cell_per_second_array);
    ClearVector(g_cell_per_img_array);
    ClearVector(g_cell_total_array);
    ClearVector(g_cell_avg_3s_array);
    ClearVector(g_cell_avg_history_array);
    ClearVector(g_img_avg_history_array);
    ClearVector(g_cell_area_array);
    ClearVector(g_cell_peri_array);
    ClearVector(g_cell_dia_array);
    ClearVector(g_cell_short_array);
    ClearVector(g_cell_long_array);
    ClearVector(g_cell_vol_array);
    ClearVector(g_cell_ell_array);
    ClearVector(g_cell_round_array);
    ClearVector(g_show_cellinfos);
    ClearVector(g_filter_cellinfos);
    g_second = 0;
    g_cell_total = 0;
    g_img_total = 0;
    g_cell_per_second_avg = 0;
    g_img_per_second_avg = 0;
    g_img_per_second = 0;
    g_cell_per_second = 0;
}
float VectorAverage(const std::vector<float>& v) {
    float avg = 0;
    for (const int& e : v) {
        avg += e;
    }
    avg /= v.size();
    return avg;
}


//拉流保存图片
void GetImageAsync(NativeCamera* in_camera) {
    
    int id = 0;
    int second = 0;
    std::string old_time_str;
    struct tm time_info;
    time_t raw_time;
    auto time_now = std::chrono::steady_clock::now();
    if (in_camera->Init()) {
        in_camera->SetParas(320, 480, 160, 171, 2000);
        in_camera->StartCapture();
    }
    while (true) {
        {
            if (std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::steady_clock::now() - time_now).count() > 1000) {
                time_now = std::chrono::steady_clock::now();
                time(&raw_time);
                localtime_s(&time_info, &raw_time);
                char time_info_buffer[100];
                strftime(time_info_buffer, 100, "%G_%m_%d_%H%M%S", &time_info);
                if (std::string(time_info_buffer) != old_time_str)
                {
                    old_time_str = std::string(time_info_buffer);
                    //std::lock_guard<std::mutex> guard(g_img_plot_mutex);
                    g_img_per_second_array.push_back(g_img_per_second);
                    g_second_array.push_back(second);
                    second++;
                    g_img_per_second = 0;
                }
                if (in_camera->SaveImages(id, second, std::string(time_info_buffer), g_b_save)) {
                    g_img_per_second++;
                }
            }
        }
    }
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
    pShowUI->Init();   //初始化
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

//
string unicode2string(LPCWSTR lps) {

    int len = WideCharToMultiByte(CP_ACP, 0, lps, -1, NULL, 0, NULL, NULL);
    if (len <= 0) {
        return "";
    }
    else {
        char* dest = new char[len];
        WideCharToMultiByte(CP_ACP, 0, lps, -1, dest, len, NULL, NULL);
        dest[len - 1] = 0;
        string str(dest);
        delete[] dest;
        return str;
    }
}

void Tchar2Char(const TCHAR* tchar, char* _char)
{
    int iLength;
    iLength = WideCharToMultiByte(CP_ACP,0,tchar,-1,NULL,0,NULL,NULL);
    WideCharToMultiByte(CP_ACP,0,tchar,-1,_char,iLength,NULL,NULL);
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
    ImGuiIO& io = ImGui::GetIO();
    io.Fonts->AddFontFromFileTTF("Microsoft-YaHei-Regular.ttc", 20,NULL, io.Fonts->GetGlyphRangesChineseSimplifiedCommon());
    //ImGuiIO& io = ImGui::GetIO(); (void)io;
    //io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;     // Enable Keyboard Controls
    //io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;      // Enable Gamepad Controls

    // Setup Dear ImGui style
    //ImGui::StyleColorsDark();
    ImGui::StyleColorsLight();
    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL3_Init(glsl_version);


    

    // capture state
    bool b_stop_capture = true;
    int pos_selected = 39;
    const char* volume_items[] = { "10  uL","20  uL","30  uL","50  uL","100 uL" };
    int volume_array[5] = { 10,20,30,50,100 };
    int volume_current = 2;
    bool b_save = false;
    bool b_save_total_imgs = false;
    bool b_start = false;
    bool b_preview = true;
    enum Speed_Element { Element_1, Element_2, Element_3, Element_COUNT };
    int speed_array[3] = { 10,20,30 };
    int speed_current_elem = Element_1;
    char img_width_cstr[128] = "320";
    char img_offset_x_cstr[128] = "160";
    char img_height_cstr[128] = "480";
    char img_exposure_cstr[128] = "171";
    char img_acquisition_frame_rate_cstr[128] = "2000";

    char laser_frequency_cstr[128] = "5000";
    char laser_width_cstr[128] = "200";
    char laser_intensity_cstr[128] = "100";
    int laser_frequency = atoi(laser_frequency_cstr);
    int laser_width = atoi(laser_width_cstr);
    int laser_intensity = atoi(laser_intensity_cstr);
    ImVec4 clear_color = ImVec4(0.45f, 0.55f, 0.60f, 1.00f);



    enum Info_Item {Item_ci, Item_cc, Item_tv, Item_s, Item_px, Item_py, Item_count };
    std::vector<std::tuple<std::string, float, std::string> > info_v = {
    std::make_tuple(u8"已采集图像",0.0,""),
    std::make_tuple(u8"已采集细胞",0.0,""),
    std::make_tuple(u8"总体积",0.0,"uL"),
    std::make_tuple(u8"流速",0.0,"uL/min"),
    std::make_tuple(u8"试管位置 x",0.0,""),
    std::make_tuple(u8"试管位置 y",0.0,""),

    };

    // load image
    GLuint tex_id_preview = 0;
    // log window
    g_LogWindow->AddLog(u8"打开成功！ \n");

    // Native camera
    NativeCamera* camera = new NativeCamera(g_LogWindow, g_detect_class_num, g_detect_preview_num);
    //if (camera->Init()) {
    //    camera->SetParas(atoi(img_width_cstr), atoi(img_height_cstr), atoi(img_offset_x_cstr), atoi(img_exposure_cstr), atoi(img_acquisition_frame_rate_cstr));
    //}

    //std::thread thread_save_img = std::thread(SaveImageAsync, camera);
    //获取相机照片
    std::thread thread_save_img = std::thread(GetImageAsync, camera);
    thread_save_img.detach();
    //if (camera->SaveAsync()) {
    //    std::thread thread_save_image = std::thread([](NativeCamera* in_camera) {
    //        in_camera->SaveImageFromQueue();
    //        }, camera);
    //    thread_save_image.detach();
    //}

    // laser controlg_b_preview
    std::string serial_name = "\\\\.\\COM4";
    LaserControl* laser = new LaserControl(serial_name.c_str(), laser_width, laser_frequency, laser_intensity, g_LogWindow);

    // is screenshot
    bool b_screenshot = true;

    // progress bar color
    ImVec4 orange_color = ImVec4(1.0, 0.43, 0.35, 1.0);
    ImVec4 green_color = ImVec4(0.6, 1.0, 0.6, 0.8);
    ImVec4 progress_bar_color = orange_color;

    // 
    int target_img_num = 6000;
    bool b_able_analyze = true;

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


        //std::cout << "camera num" << camera->GetDeviceNum() << std::endl;
        g_img_total = camera->GetTotalImageSize();
        std::get<1>(info_v[Item_ci]) = g_img_total;
        std::get<1>(info_v[Item_cc]) = g_cell_total;
        std::get<1>(info_v[Item_tv]) = volume_array[volume_current];
        std::get<1>(info_v[Item_s]) = speed_array[speed_current_elem];
        std::get<1>(info_v[Item_py]) = int(pos_selected / 8) + 1;
        std::get<1>(info_v[Item_px]) = pos_selected % 8 + 1;


        glfwPollEvents();

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
            if (ImGui::BeginMenu(u8"菜单")) {
                ImGui::EndMenu();
            }
            ImGui::EndMainMenuBar();
        }

        // capture window
        {
            ImGui::Begin("Capture", NULL, window_flags);
            int current_region_width = ImGui::GetContentRegionAvail().x;
            ImGui::PushStyleVar(ImGuiStyleVar_SelectableTextAlign, ImVec2(0.5, 0.5));

            // pos
            for (int r = 0; r < 5; r++) {
                for (int c = 0; c < 8; c++) {
                    if (c > 0)
                        ImGui::SameLine();
                    ImGui::PushID(r * 8 + c);
                    std::string AtoE[5] = { "A","B","C","D","E" };
                    std::string select_text = AtoE[r] + std::to_string(c + 1);
                    if (ImGui::Selectable(select_text.c_str(), pos_selected == (r * 8 + c), 0, ImVec2(35, 35))) {
                        pos_selected = r * 8 + c;
                    }
                    ImGui::PopID();
                }
            }
            ImGui::PopStyleVar(1);

            ImGui::Separator();
            //ImGui::PushItemWidth(-FLT_MIN);
            if (g_progress > 0.99) {
                progress_bar_color = green_color;
            }
            ImGui::PushStyleColor(ImGuiCol_PlotHistogram, progress_bar_color);
            if (b_start) {
                //g_progress = (static_cast<float>(g_second)) / (volume_array[volume_current] / speed_array[speed_current_elem] * 60);
                g_progress = camera->GetTotalImageSize()/float(target_img_num);
            }
            ImGui::ProgressBar(g_progress, ImVec2(current_region_width * 1.0f, 20));
            ImGui::ProgressBar(camera->GetAnalyzeProgress(), ImVec2(current_region_width * 1.0f, 20));
            ImGui::PopStyleColor(1);
            if (ImGui::Button(u8"进出仓", ImVec2(current_region_width * 0.5f, 40))) {
                g_flow->OpenCloseDoor();
                g_LogWindow->AddLog(u8"进出仓成功! \n");
            }
            ImGui::SameLine();
            if (ImGui::Button(u8"开始", ImVec2(current_region_width * 0.5f, 40))) {
                g_flow->CollectStart(std::get<1>(info_v[Item_tv]), std::get<1>(info_v[Item_s]), 0, std::get<1>(info_v[Item_py]), std::get<1>(info_v[Item_px]));
                g_time_start = std::chrono::steady_clock::now();
                b_screenshot = true;
                b_start = true;
                g_progress = 0.0;
                ClearPlot();
                b_able_analyze = true;
                camera->Clear();
                for (int i = 0; i < g_detect_tex_ids.size(); i++) {
                    for (int j = 0; j < g_detect_tex_ids[0].size(); j++) {
                        glDeleteTextures(1, &(g_detect_tex_ids[i][j]));
                        g_detect_tex_ids[i][j] = 0;
                    }
                }
            }
            if (ImGui::IsItemHovered()) {
                char tip_buffer[100];
                sprintf(tip_buffer, "volume: %.0f \nspeed : %.0f \npos   : %.0f %.0f \n", std::get<1>(info_v[Item_tv]), std::get<1>(info_v[Item_s]), std::get<1>(info_v[Item_py]), std::get<1>(info_v[Item_px]));
                ImGui::SetTooltip(tip_buffer);
            }
            if (ImGui::Button(u8"打开路径", ImVec2(current_region_width * 0.5f, 40)))
            {
                BROWSEINFO ofn;
                TCHAR szFile[MAX_PATH];

                ZeroMemory(&ofn, sizeof(BROWSEINFO));
                ofn.hwndOwner = NULL;
                ofn.pszDisplayName = szFile;
                ofn.lpszTitle = _T("Select the storage path:");
                ofn.ulFlags = BIF_DONTGOBELOWDOMAIN | BIF_RETURNONLYFSDIRS | BIF_NEWDIALOGSTYLE;
                LPITEMIDLIST idl = SHBrowseForFolder(&ofn);
                if (idl != NULL)
                {
                    SHGetPathFromIDList(idl, szFile);
                    g_file_path = unicode2string(szFile);
                }
                else {
                    //g_file_path = "E:\\Data\\CameraImages\\20220318";     //默认地址
                }

                

            }
            ImGui::SameLine();
            if (ImGui::Button(u8"停止", ImVec2(current_region_width * 0.5f, 40)) || camera->GetTotalImageSize() > target_img_num ) {
                b_start = false;
                //g_progress = 0.0;
                g_flow->StopCollect();
            }

            //save path
            if (g_function_type == 60 && g_para1 == 1) {
                b_save = true;
            }
            else {
                b_save = false;
            }
            ImGui::Checkbox(u8"保存细胞图片在：", &b_save);

            ImGui::SameLine();
            if (std::filesystem::exists(g_file_path)) {
                ImGui::Text(g_file_path.c_str());
            }
            else {
                ImGui::TextColored(ImVec4(1.0, 0.0, 0.0, 1.0), g_file_path.c_str());
                if (ImGui::IsItemHovered()) {
                    ImGui::SetTooltip(u8"Path not exists");
                }
            }
            //ImGui::Checkbox(u8"是否保存全部图片", &b_save_total_imgs);

            // auto stop
            if (g_progress > 0.99 && b_start == true) {
                g_progress = 1.0;
                b_start = false;
            }
            // analyze
            if (ImGui::Button(u8"分析", ImVec2(current_region_width * 0.5f, 30)) || (b_able_analyze && g_progress > 0.99)) {
                
                std::thread thread_analyze(&NativeCamera::AnalyzeImages, camera, "1", g_file_path, b_save, b_save_total_imgs);
                thread_analyze.detach();
                b_able_analyze = false;

                
                

            }
            ImGui::SameLine();
            if (ImGui::Button(u8"打开图片", ImVec2(current_region_width * 0.5f, 30))) {
                if (std::filesystem::exists(std::filesystem::path(g_file_path))) {
                    system(("start " + std::string(g_file_path)).c_str());
                }
            }
            
            // clear plot
         
            if (ImGui::Button(u8"清空", ImVec2(current_region_width * 0.5f, 30))) {
                g_progress = 0.0;
                ClearPlot();
                //清理照片
                for (int i = 0; i < g_detect_tex_ids.size(); i++) {
                    for (int j = 0; j < g_detect_tex_ids[0].size(); j++) {
                        glDeleteTextures(1, &(g_detect_tex_ids[i][j]));
                        g_detect_tex_ids[i][j] = 0;
                    }
                }
                //清空数据
                camera->Clear();
                g_LogWindow->AddLog(u8"清空成功! \n");
                }

            
            ImGui::End();
        }

        // preview window
        {
            ImGui::Begin("Preview", NULL, window_flags);   // Pass a pointer to our bool variable (the window will have a closing button that will clear the bool when clicked)
            //ImGui::Checkbox("Preview", &b_preview);
            ImGui::Image((void*)(intptr_t)tex_id_preview, ImVec2(atoi(img_width_cstr), atoi(img_height_cstr)));
            ImGui::End();
        }

        //ImPlot::ShowDemoWindow();

        // plot window

        {              

            ImGui::Begin("Plot", NULL, window_flags);   // Pass a pointer to our bool variable (the window will have a closing button that will clear the bool when clicked)
            

            
            // plot data prepare


            //std::lock_guard<std::mutex> guard(g_img_plot_mutex);
            std::vector<float> cell_per_second_avg_list(g_second_array.size(), g_cell_per_second_avg);
            std::vector<float> img_per_second_avg_list(g_second_array.size(), g_img_per_second_avg);
            // begin plot
            static ImPlotSubplotFlags flags = ImPlotSubplotFlags_NoTitle;
            static float rratios[] = { 1,1,1};
            static float cratios[] = { 1,1,1};
            static int xybins[2] = { 90,30 };
            ImGui::SliderInt2("Bins", xybins, 1, 200);  //滑动条
            ImGui::SameLine();
            // Screen shot
            if (ImGui::Button(u8"截图保存", ImVec2(100, 30))) {

                ImVec2 StatisPos = ImGui::GetWindowPos();        //获取图像窗口位置及大小
                float Statis_width = ImGui::GetWindowWidth();
                float Statis_height = ImGui::GetWindowHeight();


                //cv::Mat img(nScreenHeight, nScreenWidth, CV_8UC3);
                cv::Mat img(Statis_height-60, Statis_width, CV_8UC3);    //去掉窗口最上面一行约60


                glPixelStorei(GL_PACK_ALIGNMENT, (img.step & 3) ? 1 : 4);
                //glPixelStorei(GL_PACK_ROW_LENGTH, img.step / img.elemSize()); // 这句不加好像也没问题？
                glReadPixels(StatisPos.x, nScreenHeight- StatisPos.y- Statis_height, img.cols, img.rows, GL_BGR, GL_UNSIGNED_BYTE, img.data);
                
                cv::Mat flipped;
                cv::flip(img, flipped, 0);
                std::string desktop_path = g_file_path;
                struct tm time_info;
                time_t raw_time;
                time(&raw_time);
                localtime_s(&time_info, &raw_time);
                char time_info_buffer[100];
                strftime(time_info_buffer, 100, "Pic_%G_%m_%d_%H%M%S_res.bmp", &time_info);
                std::string res_path = (std::filesystem::path(desktop_path) / std::filesystem::path(std::string(time_info_buffer))).string();
                g_LogWindow->AddLog(u8"保存： %s \n", res_path.c_str());
                cv::imwrite(res_path, flipped);
                /*if ((g_progress > 0.99 && b_screenshot)) {
                    b_screenshot = false;
                }*/
            }


            if (ImPlot::BeginSubplots("My Subplots", 3, 3, ImVec2(-1, -1), flags, rratios, cratios)) {

                ImPlot::PushColormap("Hot");
                if (ImPlot::BeginPlot("")) {
                    ImPlot::SetupAxes(u8"直径(um)", u8"圆度(um)", ImPlotAxisFlags_AutoFit, ImPlotAxisFlags_AutoFit);
                    ImPlot::PlotHistogram2D("", g_cell_dia_array.data(), g_cell_round_array.data(), g_cell_dia_array.size(), xybins[0], xybins[1],  false, ImPlotRect(0, 0, 0,0));
                    ImPlot::EndPlot();
                }
                if (ImPlot::BeginPlot("")) {
                    ImPlot::SetupAxes(u8"直径(um)", u8"偏心率", ImPlotAxisFlags_AutoFit, ImPlotAxisFlags_AutoFit);
                    ImPlot::PlotHistogram2D("", g_cell_dia_array.data(), g_cell_ell_array.data(), g_cell_dia_array.size(), xybins[0], xybins[1],  false, ImPlotRect(0, 0, 0, 0));
                    ImPlot::EndPlot();
                }
                if (ImPlot::BeginPlot("")) {
                    ImPlot::SetupAxes(u8"长(um)", u8"短(um2)", ImPlotAxisFlags_AutoFit, ImPlotAxisFlags_AutoFit);
                    ImPlot::PlotHistogram2D("", g_cell_long_array.data(), g_cell_short_array.data(), g_cell_long_array.size(), xybins[0], xybins[1], false, ImPlotRect(0, 0, 0, 0));
                    ImPlot::EndPlot();
                 }
                ImPlot::PopColormap();

               if (ImPlot::BeginPlot("")) {
                    ImPlot::SetupAxes(u8"直径(um)", u8"数量", ImPlotAxisFlags_AutoFit, ImPlotAxisFlags_AutoFit);
                    ImPlot::SetupLegend(ImPlotLocation_SouthEast, ImPlotLegendFlags_None);
                    ImPlot::PlotHistogram("", g_cell_dia_array.data(), g_cell_dia_array.size(), 100);
                    ImPlot::EndPlot();
                }
                if (ImPlot::BeginPlot("")) {
                    ImPlot::SetupAxes(u8"周长(um)", u8"数量", ImPlotAxisFlags_AutoFit, ImPlotAxisFlags_AutoFit);
                    ImPlot::SetupLegend(ImPlotLocation_SouthEast, ImPlotLegendFlags_None);
                    ImPlot::PlotHistogram("", g_cell_peri_array.data(), g_cell_peri_array.size(), 100);
                    ImPlot::EndPlot();
                }
                if (ImPlot::BeginPlot("")) {
                    ImPlot::SetupAxes(u8"面积(um2)", u8"数量", ImPlotAxisFlags_AutoFit, ImPlotAxisFlags_AutoFit);
                    ImPlot::SetupLegend(ImPlotLocation_SouthEast, ImPlotLegendFlags_None);
                    ImPlot::PlotHistogram("", g_cell_area_array.data(), g_cell_area_array.size(), 100);
                    ImPlot::EndPlot();
                }
                if (ImPlot::BeginPlot("")) {
                    ImPlot::SetupAxes(u8"体积(um3)", u8"数量", ImPlotAxisFlags_AutoFit, ImPlotAxisFlags_AutoFit);
                    ImPlot::SetupLegend(ImPlotLocation_SouthEast, ImPlotLegendFlags_None);
                    ImPlot::PlotHistogram("", g_cell_vol_array.data(), g_cell_vol_array.size(), 100);
                    ImPlot::EndPlot();
                }
                if (ImPlot::BeginPlot("")) {
                    ImPlot::SetupAxes(u8"偏心率", u8"数量", ImPlotAxisFlags_AutoFit, ImPlotAxisFlags_AutoFit);
                    ImPlot::SetupLegend(ImPlotLocation_SouthEast, ImPlotLegendFlags_None);
                    ImPlot::PlotHistogram("", g_cell_ell_array.data(), g_cell_ell_array.size(), 50);
                    ImPlot::EndPlot();
                }
                if (ImPlot::BeginPlot("")) {
                    ImPlot::SetupAxes(u8"圆度", u8"数量", ImPlotAxisFlags_AutoFit, ImPlotAxisFlags_AutoFit);
                    ImPlot::SetupLegend(ImPlotLocation_SouthEast, ImPlotLegendFlags_None);
                    ImPlot::PlotHistogram("", g_cell_round_array.data(), g_cell_round_array.size(), 50);
                    ImPlot::EndPlot();
                }

                /*if (ImPlot::BeginPlot("")) {
                    ImPlot::SetupAxes(u8"时间(s)", NULL, ImPlotAxisFlags_AutoFit, ImPlotAxisFlags_AutoFit);
                    ImPlot::SetupLegend(ImPlotLocation_SouthEast, ImPlotLegendFlags_None);
                    ImPlot::PlotLine(u8"图像每秒", g_second_array.data(), g_img_per_second_array.data(), g_second_array.size());
                    ImPlot::EndPlot();
                }*/

                if (camera->GetTotalImageSize() == 0 && g_cell_dia_array.size() == 0) {
                    // clear display tex id 
                    for (int i = 0; i < g_detect_tex_ids.size(); i++) {
                        for (int j = 0; j < g_detect_tex_ids[0].size(); j++) {
                            glDeleteTextures(1, &(g_detect_tex_ids[i][j]));
                            g_detect_tex_ids[i][j] = 0;
                        }
                    }
                    
                    //传入形态学参数到数组

                    std::vector<CellInfo> total_cells = camera->GetTotalCells();
                    g_cell_total = total_cells.size();
                    
                    for (int i = 0; i < total_cells.size(); i++) {
                        g_cell_dia_array.push_back(total_cells[i].m_diameter);
                        g_cell_area_array.push_back(total_cells[i].m_area);
                        g_cell_peri_array.push_back(total_cells[i].m_perimeter);
                        g_cell_short_array.push_back(total_cells[i].m_shortaxis);
                        g_cell_long_array.push_back(total_cells[i].m_longaxis);
                        g_cell_vol_array.push_back(total_cells[i].m_vol);
                        g_cell_ell_array.push_back(total_cells[i].m_eccentricity);
                        g_cell_round_array.push_back(total_cells[i].m_roundness);
                        if (i > 5000) break;
                        // display total cells image 
                        if (i < g_detect_class_num * g_detect_preview_num) {
                            int y = i % g_detect_preview_num;
                            int x = i / g_detect_preview_num;
                            g_show_cellinfos.push_back(total_cells[i]);
                            LoadTextureFromImage(GrayToRGB(g_show_cellinfos[i].m_image), &(g_detect_tex_ids.at(x).at(y)));
                        }

                    }
                    
                }

                ImPlot::EndSubplots();
            }
               

            ImGui::End();
            //ImGui::PopStyleColor(11);


        }

        //filter window,用户自定义设定参数过滤
        {
        ImGui::Begin("filter", NULL, window_flags);
        ImGui::Text(u8"筛选自定义设定");
        ImGui::Separator();

        //IMGUI_DEMO_MARKER("Help");
        if (ImGui::CollapsingHeader(u8"条件添加"))
        {
            Condition m_condition;
            
            float c_min;
            float c_max;

            
            ImGui::Combo(u8"条件选择", &g_property_int, u8"直径\0面积\0周长\0体积\0偏心率\0圆度\0");
            m_condition.m_property = (Enum_property)g_property_int;

            static float set_min = 0.001f;
            static float set_max = 0.001f;

            ImGui::InputFloat(u8"最小", &set_min, 0.01f, 1.0f, "%.3f");
            ImGui::InputFloat(u8"最大", &set_max, 0.01f, 1.0f, "%.3f");

            ImGui::Combo(u8"条件", &g_logic, u8"与\0或\0");
            m_condition.m_min = set_min;
            m_condition.m_max = set_max;
            m_condition.m_b_and = g_logic;

            if (ImGui::Button(u8"确认添加", ImVec2(90, 25))) {
                g_conditions.push_back(m_condition);
                g_LogWindow->AddLog(u8"条件添加成功！\n");
            }
            ImGui::SameLine();
            if (ImGui::Button(u8"清空条件", ImVec2(90, 25))) {
                ClearVector(g_conditions);
                g_LogWindow->AddLog(u8"条件清空成功！\n");
            }

        }

        ImGui::Separator();
        ImGui::Combo(u8"是否保存", &g_set_filter_store, u8"不保存\0保存\0");
        if (g_set_filter_store == 1)
        {
            if (ImGui::Button(u8"选择路径", ImVec2(80, 40)))
            {
                BROWSEINFO ofn;
                TCHAR szFile[MAX_PATH];

                ZeroMemory(&ofn, sizeof(BROWSEINFO));
                ofn.hwndOwner = NULL;
                ofn.pszDisplayName = szFile;
                ofn.lpszTitle = _T("选择文件夹:");
                ofn.ulFlags = BIF_DONTGOBELOWDOMAIN | BIF_RETURNONLYFSDIRS | BIF_NEWDIALOGSTYLE; //包含新建文件夹功能
                LPITEMIDLIST idl = SHBrowseForFolder(&ofn);
                if (idl != NULL)
                {
                    SHGetPathFromIDList(idl, szFile);
                    /*CHAR szFile_char[MAX_PATH];
                    Tchar2Char(szFile,szFile_char);*/
                    g_img_filter_path = unicode2string(szFile);
                }

            }
            ImGui::Text(u8"保存路径:");
            ImGui::SameLine;
            ImGui::Text(g_img_filter_path.c_str());
        }

        ImGui::Separator();
        // 执行筛选函数
        if (ImGui::Button(u8"执行筛选", ImVec2(150, 40))) {
            ClearVector(g_filter_cellinfos);
            ClearVector(g_show_cellinfos);
            for (int i = 0; i < g_detect_tex_ids.size(); i++) {
                for (int j = 0; j < g_detect_tex_ids[0].size(); j++) {
                    glDeleteTextures(1, &(g_detect_tex_ids[i][j]));
                    g_detect_tex_ids[i][j] = 0;
                }
            }

            //传入数据
            std::vector<CellInfo> cell_infos = camera->GetTotalCells();
            int filter_cell_num = 0;
            for (int i = 0; i < cell_infos.size(); i++) {
                bool b_cell_filter_total = false;
                for (int j = 0; j < g_conditions.size(); j++) {
                    bool b_cell_filter_single = true;
                    switch (g_conditions[j].m_property) {
                        case Enum_property::Enum_diameter:
                            b_cell_filter_single = (cell_infos[i].m_diameter >= g_conditions[j].m_min && cell_infos[i].m_diameter <= g_conditions[j].m_max);
                            break;
                        case Enum_property::Enum_area:
                            b_cell_filter_single = (cell_infos[i].m_area >= g_conditions[j].m_min && cell_infos[i].m_area <= g_conditions[j].m_max);
                            break;
                        case Enum_property::Enum_perimeter:
                            b_cell_filter_single = (cell_infos[i].m_perimeter >= g_conditions[j].m_min && cell_infos[i].m_perimeter <= g_conditions[j].m_max);
                            break;
                        case Enum_property::Enum_volume:
                            b_cell_filter_single = (cell_infos[i].m_vol >= g_conditions[j].m_min && cell_infos[i].m_vol <= g_conditions[j].m_max);
                            break;
                        case Enum_property::Enum_eccentricity:
                            b_cell_filter_single = (cell_infos[i].m_eccentricity >= g_conditions[j].m_min && cell_infos[i].m_eccentricity <= g_conditions[j].m_max);
                            break;
                        case Enum_property::Enum_roundness:
                            b_cell_filter_single = (cell_infos[i].m_roundness >= g_conditions[j].m_min && cell_infos[i].m_roundness <= g_conditions[j].m_max);
                            break;

                    }
                    if (j == 0) {
                        if (g_conditions[j].m_b_and == 0) {
                            b_cell_filter_total = true;
                        }
                        else if (g_conditions[j].m_b_and == 1) {
                            b_cell_filter_total = false;
                        }
                    }
                    if (g_conditions[j].m_b_and == 0) {
                        // and
                        b_cell_filter_total = b_cell_filter_total && b_cell_filter_single;
                    }
                    else if(g_conditions[j].m_b_and == 1){
                        // or
                        b_cell_filter_total = b_cell_filter_total || b_cell_filter_single;
                    }
                }
                if (b_cell_filter_total) {
                    g_filter_cellinfos.push_back(cell_infos[i]);
                    if (filter_cell_num < g_detect_class_num * g_detect_preview_num) {
                        int y = filter_cell_num % g_detect_preview_num;
                        int x = filter_cell_num / g_detect_preview_num;
                        LoadTextureFromImage(GrayToRGB(cell_infos[i].m_image), &(g_detect_tex_ids.at(x).at(y)));
                    }
                    filter_cell_num++;
                    //保存筛选图片
                    if (g_set_filter_store) {
                        std::string save_filter_cell_path = (std::filesystem::path(g_img_filter_path)/std::filesystem::path(cell_infos[i].m_name)).string();
                        cv::imwrite(save_filter_cell_path.c_str(),cell_infos[i].m_image);
                    }
                }
            }
        }

        ImGui::Separator();
        ImGui::End();
        }
        // detect window
        {
            ImGui::Begin("detect", NULL, window_flags);
            ImGui::BeginChild("scrolling", ImVec2(0, 0), false, ImGuiWindowFlags_HorizontalScrollbar);
            for (int i = 0; i < g_conditions.size(); i++)
            {
                std::string bottonname = std::to_string(g_conditions[i].m_min)+"<"+ g_condition_names.at(g_conditions[i].m_property) + "<"+ std::to_string(g_conditions[i].m_max);
                if (ImGui::Button(bottonname.c_str()))
                {
                    g_conditions.erase(g_conditions.begin()+i);
                    g_LogWindow->AddLog(u8"删除条件：%s\n", bottonname.c_str());
                }
                if (i < g_conditions.size() - 1) {
                    ImGui::SameLine();
                }
                else {
                    ImGui::Separator();
                }
            }
            //添加属性值
            for (int i = 0; i < g_detect_class_num; i++) {
                for (int j = 0; j < g_detect_preview_num; j++) {
                    ImGui::Image((void*)(intptr_t)g_detect_tex_ids[i][j], ImVec2(g_detect_img_width, g_detect_img_height));

                    if (g_show_cellinfos.size() != 0 && g_filter_cellinfos.size() ==0) {
                        if (i * g_detect_preview_num + j < g_show_cellinfos.size()) {
                            if (ImGui::IsItemHovered()) {
                                char tip_buffer[100];
                                sprintf(tip_buffer, u8"直径: %.3f \n面积: %.3f \n周长: %.3f\n体积：%.3f\n偏心率： %.3f \n圆度：%.3f \n", g_show_cellinfos[i * g_detect_preview_num + j].m_diameter, g_show_cellinfos[i * g_detect_preview_num + j].m_perimeter, g_show_cellinfos[i * g_detect_preview_num + j].m_area, g_show_cellinfos[i * g_detect_preview_num + j].m_vol, g_show_cellinfos[i * g_detect_preview_num + j].m_eccentricity, g_show_cellinfos[i * g_detect_preview_num + j].m_roundness);
                                ImGui::SetTooltip(tip_buffer);
                            }
                        }
                    }
                    

                    else if (i * g_detect_preview_num + j < g_filter_cellinfos.size()) {
                        if (ImGui::IsItemHovered()) {
                            char tip_buffer[100];
                            sprintf(tip_buffer, u8"直径: %.3f \n面积: %.3f \n周长: %.3f\n体积：%.3f\n偏心率： %.3f \n圆度：%.3f \n", g_filter_cellinfos[i * g_detect_preview_num + j].m_diameter, g_filter_cellinfos[i * g_detect_preview_num + j].m_perimeter, g_filter_cellinfos[i * g_detect_preview_num + j].m_area, g_filter_cellinfos[i * g_detect_preview_num + j].m_vol, g_filter_cellinfos[i * g_detect_preview_num + j].m_eccentricity, g_filter_cellinfos[i * g_detect_preview_num + j].m_roundness);
                            ImGui::SetTooltip(tip_buffer);
                        }
                    }
                    
                    ImGui::SameLine();
                }
                ImGui::NewLine();
            }

            ImGui::Separator();
            std::string btn_text = u8"打开筛选细胞图，一共" + std::to_string(g_filter_cellinfos.size()) + u8"个";
            if (ImGui::Button(btn_text.c_str())) {
                if (std::filesystem::exists(std::filesystem::path(g_img_filter_path))) {
                    system(("start " + std::string(g_img_filter_path)).c_str());
                }
                else {
                    g_LogWindow->AddLog("file : %s not exists !! \n", g_img_filter_path.c_str());
                }
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
                if (ImGui::BeginTabItem(u8"信息"))
                {
                    static ImGuiTableFlags flags = ImGuiTableFlags_Resizable | ImGuiTableFlags_Reorderable | ImGuiTableFlags_Hideable | ImGuiTableFlags_BordersOuter | ImGuiTableFlags_BordersV;
                    if (ImGui::BeginTable(u8"信息", 2, flags))
                    {
                        ImGui::TableSetupColumn(u8"属性");
                        ImGui::TableSetupColumn(u8"值");
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
                    ImGui::Separator();
                    ImGui::Text(u8"g_progress %f", g_progress);
                    //ImGui::Text("b screenshot %d", b_screenshot);
                    ImGui::Text("b start %d", b_start);
                    ImGui::Text("g b save %d", g_b_save);
                    ImGui::Text("b save %d", b_save);
                    //ImGui::Text(u8"fps: %d", g_img_per_second);
                    ImGui::Separator();
                    ImGui::Text("%.3f ms/frame (%.1f FPS)", 1000.0f / ImGui::GetIO().Framerate, ImGui::GetIO().Framerate);
                    ImGui::Separator();
                    ImGui::EndTabItem();
                }
                // properties tab
                if (ImGui::BeginTabItem(u8"属性"))
                {
                    //flow setting
                    ImGui::Separator();
                    ImGui::Text(u8"液流");
                    ImGui::Combo(u8"选择体积", &volume_current, volume_items, IM_ARRAYSIZE(volume_items));
                    // speed slider
                    const char* elems_names[Element_COUNT] = { "10 uL/min","20 uL/min","30 uL/min" };
                    const char* elem_name = (speed_current_elem >= 0 && speed_current_elem < Element_COUNT) ? elems_names[speed_current_elem] : "Unknown";
                    ImGui::SliderInt(u8"选择流速", &speed_current_elem, 0, Element_COUNT - 1, elem_name);
                    // image setting
                    ImGui::Separator();
                    ImGui::Text(u8"相机");
                    if (ImGui::Button(u8"更新相机设置")) {
                        if (!g_b_save) {
                            if (camera->SetParas(atoi(img_width_cstr), atoi(img_height_cstr), atoi(img_offset_x_cstr), atoi(img_exposure_cstr), atoi(img_acquisition_frame_rate_cstr))) {
                                std::cout << "set paras success " << std::endl;
                            }
                            else {
                                std::cout << "set paras failed " << std::endl;
                            }
                        }
                    }
                    ImGui::InputText(u8"宽度", img_width_cstr, IM_ARRAYSIZE(img_width_cstr));
                    ImGui::InputText(u8"高度", img_height_cstr, IM_ARRAYSIZE(img_height_cstr));
                    ImGui::InputText(u8"偏移", img_offset_x_cstr, IM_ARRAYSIZE(img_offset_x_cstr));
                    ImGui::InputText(u8"同步", img_exposure_cstr, IM_ARRAYSIZE(img_exposure_cstr));
                    ImGui::InputText(u8"FPS", img_acquisition_frame_rate_cstr, IM_ARRAYSIZE(img_acquisition_frame_rate_cstr));
                    // laser setting
                    ImGui::Separator();
                    ImGui::Text(u8"激光");
                    if (ImGui::Button(u8"重连")) {
                        delete laser;
                        laser = new LaserControl(serial_name.c_str(), laser_width, laser_frequency, laser_intensity, g_LogWindow);
                    }
                    ImGui::SameLine();
                    if (ImGui::Button(u8"打开")) {
                        laser->OpenLaser();
                        Sleep(10);
                        laser->OpenLaser();
                    }
                    ImGui::SameLine();
                    if (ImGui::Button(u8"关闭")) {
                        laser->CloseLaser();
                    }
                    ImGui::SameLine();
                    if (ImGui::Button(u8"更新")) {
                        laser_width = atoi(laser_width_cstr);
                        laser_frequency = atoi(laser_frequency_cstr);
                        laser_intensity = atoi(laser_intensity_cstr);
                        laser->SetParas(laser_width, laser_frequency, laser_intensity);
                    }
                    ImGui::InputText(u8"脉冲", laser_frequency_cstr, IM_ARRAYSIZE(laser_frequency_cstr));
                    ImGui::InputText(u8"脉宽(ns)", laser_width_cstr, IM_ARRAYSIZE(laser_width_cstr));
                    ImGui::InputText(u8"强度(0-200）", laser_intensity_cstr, IM_ARRAYSIZE(laser_intensity_cstr));

                    //style 
                    ImGui::Separator();
                    ImGui::Text(u8"风格");
                    static int color_idx = 1;
                    if (ImGui::Combo(u8"颜色", &color_idx, "Dark\0Light\0Classic\0"))
                    {
                        switch (color_idx)
                        {
                        case 0: ImGui::StyleColorsDark(); break;
                        case 1: ImGui::StyleColorsLight(); break;
                        case 2: ImGui::StyleColorsClassic(); break;
                        }
                    }
                    ImGui::EndTabItem();

                }
                //maintain
                if (ImGui::BeginTabItem(u8"维护")) {
                    ImGui::Text(u8"维护");
                    if (ImGui::Button(u8"过滤器充灌", ImVec2(200, 25))) {
                        g_flow->Maintain((int)3);
                    }
                    if (ImGui::Button(u8"柱塞器充灌", ImVec2(200, 25))) {
                        g_flow->Maintain((int)4);
                    }
                    if (ImGui::Button(u8"整流腔充灌", ImVec2(200, 25))) {
                        g_flow->Maintain((int)5);
                    }
                    if (ImGui::Button(u8"样本通道清洗", ImVec2(200, 25))) {
                        g_flow->Maintain((int)6);
                    }
                    if (ImGui::Button(u8"采用通道排堵", ImVec2(200, 25))) {
                        g_flow->Maintain((int)7);
                    }
                    if (ImGui::Button(u8"液路初始化", ImVec2(200, 25))) {
                        g_flow->Maintain((int)8);
                    }
                    if (ImGui::Button(u8"整机排空", ImVec2(200, 25))) {
                        g_flow->Maintain((int)11);
                    }
                    if (ImGui::Button(u8"整机充灌", ImVec2(200, 25))) {
                        g_flow->Maintain((int)12);
                    }
                    if (ImGui::Button(u8"更换PBS液", ImVec2(200, 25))) {
                        g_flow->Maintain((int)13);
                    }
                    if (ImGui::Button(u8"更换清洗液", ImVec2(200, 25))) {
                        g_flow->Maintain((int)14);
                    }
                    ImGui::EndTabItem();
                }

                // test tab
                if (ImGui::BeginTabItem(u8"测试")) {
                    if (ImGui::Button(u8"清理")) {
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
                        
                 //ImGui::ShowDemoWindow();
                    
                    if (ImGui::Button("open file")) {
                        if (std::filesystem::exists(std::filesystem::path(g_file_path))) {
                            std::string cmd_str = "start " + g_file_path;
                            system(cmd_str.c_str());
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
        glClearColor(0.5f, 0.5f, 0.5f, 1.0f);              //设置清理的颜色，即背景颜色
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


