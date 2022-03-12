#include "NativeCamera.h"
NativeCamera::NativeCamera(LogWindows* pLogWindow,int in_class_num,int in_detect_image_preview_num)
{
	m_class_num = in_class_num;
	m_detect_image_preview_num = in_detect_image_preview_num;
	pStreamSource = NULL;
	m_celldetect = CellDetect();
	detect_images = std::vector<std::queue<cv::Mat>>(m_class_num,std::queue<cv::Mat>());
	m_log_window = pLogWindow;
}
NativeCamera::NativeCamera() {
}
NativeCamera::~NativeCamera()
{
}
bool NativeCamera::SetParas(int inWidth,int inHeight,int inOffsetX,double inExposureTime,double inAcquisitionFrameRate) {
	if (pCamera != NULL) {
		return (
		modifyCameraOffsetX(pCamera, 0) == 0 &&
		modifyCameraWidth(pCamera, inWidth) == 0 &&
		modifyCameraOffsetX(pCamera, inOffsetX) == 0 &&
		modifyCameraHeight(pCamera, inHeight) == 0 &&
		modifyCameraExposureTime(pCamera, inExposureTime) == 0 &&
		modifyCameraAcquisitionFrameRate(pCamera, inAcquisitionFrameRate) == 0
		);
	}
	else {
		m_log_window->AddLog("no camera when set paras\n");
		return false;
	}
}
bool NativeCamera::StartCapture() {
	if (pCamera != NULL && pStreamSource == NULL) {
		std::lock_guard<std::mutex> guard(steamsource_mutex);

		int ret = GENICAM_CreateStreamSource(pCamera, &pStreamSource);
		if ((ret != 0) || (NULL == pStreamSource)) {
			m_log_window->AddLog("create stream obj  fail.\r\n");
			//getchar();
			return false;
		}
		// 开始拉流  
		// Start grabbing    
		ret = GENICAM_startGrabbing(pStreamSource);
		if (ret != 0) {
			m_log_window->AddLog("StartGrabbing  fail.\n");
			//注意：需要释放pStreamSource内部对象内存    
			//Attention: should release pStreamSource internal object before return     
			pStreamSource->release(pStreamSource);
			pStreamSource = NULL;
			//getchar();
			return false;
		}
		m_log_window->AddLog("start capture\n");
		return true;
	}
	else {
		m_log_window->AddLog("no camera when start capture \n");
		return false;
	}
}
bool NativeCamera::StopCapture() {
	if (pCamera != NULL && pStreamSource != NULL) {
		std::lock_guard<std::mutex> guard(steamsource_mutex);

		int ret = GENICAM_stopGrabbing(pStreamSource);
		if (ret != 0) {
			m_log_window->AddLog("StopGrabbing  fail.\n");
			//注意：需要释放pStreamSource内部对象内存    
			//Attention: should release pStreamSource internal object before return     
			pStreamSource->release(pStreamSource);
			//getchar();
			return false;
		}
		pStreamSource->release(pStreamSource);
		pStreamSource = NULL;

		m_log_window->AddLog("stop capture\n");
		return true;
	}
	else {
		m_log_window->AddLog("no camear when stop capture \n");
		return false;
	}
}
void NativeCamera::SaveImageFromQueue() {
	std_time time_start = std::chrono::system_clock::now();
	while (true) {
		if (!m_bSave || std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now() - time_start).count() > image_save_interval) {
			cv::Mat image;
			std::string path = "";
			{
				std::lock_guard<std::mutex> guard(image_mutex);
				if (!image_queue.empty()) {
					auto ele = image_queue.front();
					path = ele.second;
					image = ele.first;
					m_log_window->AddLog("get image queue size : %d \n",image_queue.size());
					image_queue.pop();
				}
			}
			if (!image.empty() && path.size() > 0) {
				std::cout << "saving : " << path << std::endl;
				cv::imwrite(path, image);
			}
			image.release();
			time_start = std::chrono::system_clock::now();
		}
	}
}
void NativeCamera::InsertImageQueue(cv::Mat image, const std::string& imagePath) {
	{
		std::lock_guard<std::mutex> guard(image_mutex);
		if (!image.empty() && imagePath.size() > 0) {
			image_queue.push(std::make_pair(image, imagePath));
			m_log_window->AddLog("set image queue size : %d \n",image_queue.size());
		}
	}
}

cv::Mat NativeCamera::OperateImageQueue(cv::Mat inImage,bool bInsert) {
	std::lock_guard<std::mutex> guard(images_mutex);
	if (bInsert) {
		//std::cout << "insert " << std::endl;
		while (!images_queue.empty()) {
			images_queue.pop();
		}
		images_queue.push(inImage);
	}
	else {
		//std::cout << "get  " << std::endl;
		if (!images_queue.empty()) {
			return images_queue.front();
		}
	}
	return cv::Mat();
}

std::vector<cv::Mat> NativeCamera::OperateDetectImageQueue(cv::Mat inImage, bool bInsert, int in_class_index) {
	std::lock_guard<std::mutex> guard(detect_images_mutex);
	std::vector<cv::Mat> res;
	if (bInsert) {
		//std::cout << "insert " << std::endl;
		detect_images.at(in_class_index).push(inImage);
		while (detect_images[in_class_index].size() > 5) {
			detect_images[in_class_index].pop();
		}
	}
	else {
		//std::cout << "get  " << std::endl;
		while (!detect_images.at(in_class_index).empty()) {
			res.push_back(detect_images[in_class_index].front());
			detect_images[in_class_index].pop();
		}
	}
	return res;
}



bool NativeCamera::Init()
{
	int32_t ret;
	GENICAM_System* pSystem = NULL;
	GENICAM_Camera* pCameraList = NULL;
	GENICAM_AcquisitionControl* pAcquisitionCtrl = NULL;
	uint32_t cameraCnt = 0;
	HANDLE threadHandle;
	unsigned threadID;
	int cameraIndex = -1;
	ret = GENICAM_getSystemInstance(&pSystem);
	if (-1 == ret) {
		m_log_window->AddLog("pSystem is null.\r\n");
		getchar();
		return false;
	}    // 发现设备     // discover camera     
	ret = pSystem->discovery(pSystem, &pCameraList, &cameraCnt, typeAll);
	if (-1 == ret) {
		m_log_window->AddLog("discovery device fail.\r\n");
		//getchar();
		return false;
	}
	if (cameraCnt < 1) {
		m_log_window->AddLog("no camera when init\r\n");
		//getchar();
		return false;
	}
	// 打印相机基本信息（序号,类型,制造商信息,型号,序列号,用户自定义ID,IP地址）     
	// Print camera info (Index, Type, Vendor, Model, Serial number, DeviceUserID, IP Address)     
	//displayDeviceInfo(pCameraList, cameraCnt);
	
	// 选择需要连接的相机     
	// Select one camera to connect to      
	//cameraIndex = selectDevice(cameraCnt);
	cameraIndex = 0;
	pCamera = &pCameraList[cameraIndex];
	// 连接设备     
	// Connect to camera     
	ret = GENICAM_connect(pCamera);
	if (0 != ret) {
		m_log_window->AddLog("connect camera failed.\n");
		//getchar();
		return false;
	}
	else {
		m_log_window->AddLog("connect success ! \n");
	}
	
	return true;
}
bool NativeCamera::BSaveValid() {
	std::lock_guard<std::mutex> guard(steamsource_mutex);
	return pStreamSource == NULL? false:true;
}
void createAlphaMat(cv::Mat& mat) // 创建一个图像
{
	int randNum = rand() % UCHAR_MAX;
	for (int i = 0; i < mat.rows; i++)
	{
		for (int j = 0; j < mat.cols; j++)
		{
			mat.at<uchar>(i, j) = (randNum + 100*i + j)%UCHAR_MAX;
		}
	}
}

int NativeCamera::GetFrame(bool bSave, bool bSaveAsync, const std::string inSavePath) {
	int cellNum = 0;
	m_bSave = bSave;
	auto time_start = std::chrono::system_clock::now();
	bool b_time_to_preview = false;
	if (std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now() - m_time_old).count() > image_preview_interval) {
		b_time_to_preview = true;
		m_time_old = std::chrono::system_clock::now();
	}
	if (bSave || b_time_to_preview) {
		int32_t ret = -1;
		uint64_t blockId = 0;
		GENICAM_Frame* pFrame;
		std_time ts = std::chrono::system_clock::now();
		{
			std::lock_guard<std::mutex> guard(steamsource_mutex);
			if (NULL == pStreamSource) {
				m_log_window->AddLog("pstream source is NULL \n");
				return cellNum;
			}
			ret = pStreamSource->getFrame(pStreamSource, &pFrame, 1);
			if (ret < 0) {
				m_log_window->AddLog("getFrame  fail.\n");
				//getchar();
				return cellNum;
			}
		}
		ret = pFrame->valid(pFrame);
		if (ret < 0) {
			m_log_window->AddLog("frame is invalid!\n");
			pFrame->release(pFrame);
			return cellNum;
		}
		cv::Mat image = cv::Mat(pFrame->getImageHeight(pFrame),
			pFrame->getImageWidth(pFrame),
			CV_8U,
			(uint8_t*)((pFrame->getImage(pFrame)))
		);
		std::vector<cv::Mat> cellImages = m_celldetect.getResult(image);
		
		cellNum = cellImages.size();

		for (int i = 0; i < cellImages.size(); i++) {
			std::string elePath = inSavePath.substr(0,inSavePath.size()-4)+std::to_string(i) + ".bmp";
			cv::imwrite(elePath,cellImages[i]);
		}
		if (b_time_to_preview) {
			OperateImageQueue(image.clone(), true);
			for (int i = 0; i < cellImages.size(); i++) {
				OperateDetectImageQueue(cellImages[i].clone(), true, Classify(cellImages[i]));
			}
		}
		if (bSave) {
			if (bSaveAsync) {
				InsertImageQueue(image.clone(), inSavePath);
			}
			else {
				//cv::imwrite(inSavePath, image);
			}
		}
		image.release();
		pFrame->release(pFrame);
	}
	return cellNum;
}
bool NativeCamera::SaveAsync() {
	return true;
	return m_bSaveAsync;
}
int NativeCamera::Classify(cv::Mat in_image) {
	// to do: classify 
	return 0;
}
