#ifndef NATIVECAMERA_H
#define NATIVECAMERA_H

#include <opencv2/videoio.hpp>
#include <opencv2/opencv.hpp>
#include "NativeUtil.h"
#include <iostream>
#include <chrono>
#include <stdlib.h>
#include "LogWindows.h"
#include "CellDetect.h"
using namespace cv;

using namespace std::literals;
typedef std::chrono::time_point<std::chrono::system_clock, std::chrono::duration<long long int, std::ratio<1, 1000000000>>> std_time;
typedef std::unordered_map<int, double> CameraParameters;
class NativeCamera {
public:
	NativeCamera();
	NativeCamera(LogWindows* pLogWindow);
	~NativeCamera();
	bool Init();
	uint32_t GetDeviceNum();
	int GetFrame(bool bSave,bool bSaveAsync,const std::string inSavePath);
	bool SetParas(int inWidth, int inHeight, int inOffsetX,double inExposureTime, double inAcquisitionFrameRate);
	bool StartCapture();
	bool StopCapture();
	bool BSaveValid();
	void SaveImageFromQueue();
private:
	CellDetect m_celldetect;
	std::string m_save_path;
	int m_num;
	bool GetFrame();
	GENICAM_StreamSource* pStreamSource;
	GENICAM_Camera* pCamera = NULL;
	std::mutex steamsource_mutex;
	std::mutex image_path_mutex;
	std::mutex image_mutex;
	std::mutex images_mutex;
	std::queue<std::string> image_path_queue;
	std_time timeOld = std::chrono::system_clock::now();
	int image_preview_interval = 100;
	int image_save_interval = 100;
	std::queue<std::pair<cv::Mat,std::string> > image_queue;
	std::queue<cv::Mat> images_queue;
	bool m_bSaveAsync = false;
	bool m_bSave = false;
	LogWindows* m_log_window;

public:
	void InsertImageQueue(cv::Mat image, const std::string& imagePath);
	bool SaveAsync();
	cv::Mat OperateImageQueue(cv::Mat inImage, bool bInsert);

};



#endif