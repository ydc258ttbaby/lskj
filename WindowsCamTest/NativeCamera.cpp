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
		while (detect_images[in_class_index].size() > m_detect_image_preview_num) {
			detect_images[in_class_index].pop();
		}
	}
	else {
		//std::cout << "get  " << std::endl;
		while (!detect_images.at(in_class_index).empty()) {
			res.push_back(detect_images[in_class_index].back());
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
	//std::lock_guard<std::mutex> guard(steamsource_mutex);
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

std::vector<CellInfo> NativeCamera::GetFrame(bool bSave, bool bSaveAsync, const std::string inSaveName, const std::string inSaveRootPath) {
	int cellNum = 0;
	std::vector<CellInfo> cellInfos;
	m_bSave = bSave;
	auto time_start = std::chrono::system_clock::now();
	bool b_time_to_preview = false;
	if (std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now() - m_time_old).count() > image_preview_interval) {
		b_time_to_preview = true;
		m_time_old = std::chrono::system_clock::now();
	}
	if (bSave || b_time_to_preview) {

		int32_t ret = 0;
		uint64_t blockId = 0;
		GENICAM_Frame* pFrame;
		std_time ts = std::chrono::system_clock::now();

		{
			//std::lock_guard<std::mutex> guard(steamsource_mutex);
			if (NULL == pStreamSource)
			{
				//m_log_window->AddLog("pstream source is NULL \n");
				//Sleep(100);
				//this->StopCapture();
				//this->StartCapture();
				//if (NULL == pStreamSource) 
			return cellInfos;
				//else
				//	m_log_window->AddLog("pstream source is good \n");

			}
			else {
				if (pStreamSource != NULL) {
					ret = pStreamSource->getFrame(pStreamSource, &pFrame, 1);
					if (ret < 0) {
						m_log_window->AddLog("getFrame  fail.\n");
						this->StopCapture();
						Sleep(100);
						this->StartCapture();
						//getchar();
						return cellInfos;
					}

					ret = pFrame->valid(pFrame);
					if (ret < 0) {
						m_log_window->AddLog("frame is invalid!\n");
						pFrame->release(pFrame);
						return cellInfos;
					}
					cv::Mat image = cv::Mat(pFrame->getImageHeight(pFrame),
						pFrame->getImageWidth(pFrame),
						CV_8U,
						(uint8_t*)((pFrame->getImage(pFrame)))
					);
					cellInfos = m_celldetect.GetResult(image);

					
					cellNum = cellInfos.size();

					if (bSave) {
						for (int i = 0; i < cellInfos.size(); i++) {
							std::filesystem::path real_save_root_path = std::filesystem::path(inSaveRootPath + "_" + std::to_string(Classify(cellInfos[i].m_image)));
							if (!std::filesystem::is_directory(real_save_root_path)) {
								std::filesystem::create_directories(real_save_root_path);
							}
							real_save_root_path /= std::filesystem::path(inSaveName.substr(0, inSaveName.size() - 4) + std::to_string(i) + ".bmp");
							//cv::imwrite(real_save_root_path.string(), cellInfos[i].m_image);
						}
					}
					if (b_time_to_preview) {
						OperateImageQueue(image.clone(), true);
						for (int i = 0; i < cellInfos.size(); i++) {
							OperateDetectImageQueue(cellInfos[i].m_image.clone(), true, Classify(cellInfos[i].m_image));
						}
					}
					if (bSave) {
						if (bSaveAsync) {
							InsertImageQueue(image.clone(), (std::filesystem::path(inSaveRootPath) / std::filesystem::path(inSaveName)).string());
						}
						else {
							//cv::imwrite((std::filesystem::path(inSaveRootPath)/std::filesystem::path(inSaveName)).string(), image);
						}
					}
					image.release();
					pFrame->release(pFrame);
				}
			}
		}
	}
	return cellInfos;
}

bool NativeCamera::SaveAsync() {
	return true;
	return m_bSaveAsync;
}
int NativeCamera::Classify(cv::Mat in_image) {
	// to do: classify 
	return 0;
}
bool NativeCamera::SaveImages(int inId, int inSecond, const std::string inName, bool inBSave) {

	bool b_time_to_preview = false;
	if (std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now() - m_time_old).count() > image_preview_interval) {
		b_time_to_preview = true;
		m_time_old = std::chrono::system_clock::now();
	}
	int32_t ret = 0;
	uint64_t blockId = 0;
	GENICAM_Frame* pFrame;
	std_time ts = std::chrono::system_clock::now();
	{
		if (NULL == pStreamSource)
		{
			return false;
		}

		else {
			if (pStreamSource != NULL) {
				ret = pStreamSource->getFrame(pStreamSource, &pFrame, 1);
				if (ret < 0) {
					m_log_window->AddLog("getFrame  fail.\n");
					this->StopCapture();
					Sleep(1);
					this->StartCapture();
					return false;
				}

				ret = pFrame->valid(pFrame);
				if (ret < 0) {
					m_log_window->AddLog("frame is invalid!\n");
					pFrame->release(pFrame);
					return false;
				}
				cv::Mat image = cv::Mat(pFrame->getImageHeight(pFrame),
					pFrame->getImageWidth(pFrame),
					CV_8U,
					(uint8_t*)((pFrame->getImage(pFrame)))
				);

				if (inBSave ) {
					ImageInfo image_info;
					image_info.m_image = image.clone();
					image_info.m_id = inId;
					image_info.m_name = inName;
					image_info.m_second = inSecond;
					m_total_images.push_back(image_info);
				}

				if (b_time_to_preview) {
					OperateImageQueue(image.clone(), true);
				}
				image.release();
				pFrame->release(pFrame);
			}
		}
	}
	return true;

}
void NativeCamera::AnalyzeImages(const std::string inSampleName, const std::string inRootPath) {
	m_total_cells.reserve(10000);
	int cell_id = 0;
	for (int i = 0; i < m_total_images.size(); ++i) {
		m_analyze_progress = static_cast<float>(i)/ m_total_images.size();
		std::vector<CellInfo> cellInfos = m_celldetect.GetResult(m_total_images[i].m_image);
		for (int j = 0; j < cellInfos.size(); ++j) {
			CellInfo cell_info = cellInfos[j];
			cell_info.m_second = m_total_images[i].m_second;
			cell_info.m_id = cell_id;
			cell_info.m_class = Classify(cell_info.m_image);
			cell_info.m_name = inSampleName + "_" + std::to_string(cell_id) + "_" + std::to_string(cell_info.m_class) + "_" + m_total_images[i].m_name + ".bmp";
			m_total_cells.push_back(cell_info);
			cell_id++;

			std::filesystem::path root_path = std::filesystem::path(inRootPath)/std::filesystem::path(std::to_string(cell_info.m_class));
			if (!std::filesystem::exists(root_path)) {
				std::filesystem::create_directories(root_path);
			}
			std::filesystem::path save_path = root_path /std::filesystem::path(cell_info.m_name);
			m_log_window->AddLog("save img in %s \n", save_path.string().c_str());
			cv::imwrite(save_path.string(),cell_info.m_image);
		}
		//std::filesystem::path root_path = std::filesystem::path(inRootPath) / std::filesystem::path(std::to_string(1));
		//if (!std::filesystem::exists(root_path)) {
		//	std::filesystem::create_directories(root_path);
		//}
		//std::filesystem::path save_path = root_path / std::filesystem::path(m_total_images[i].m_name + std::to_string(i) + ".bmp");
		//cv::imwrite(save_path.string(), m_total_images[i].m_image);
	}
	std::vector<ImageInfo> temp;
	m_total_images.swap(temp);
}
float NativeCamera::GetAnalyzeProgress() {
	return m_analyze_progress;
}
int NativeCamera::GetTotalImageSize() {
	return m_total_images.size();
}
std::vector<CellInfo> NativeCamera::GetTotalCells() {
	std::vector<CellInfo> temp;
	m_total_cells.swap(temp);
	return temp;
}
