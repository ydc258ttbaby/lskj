#include<iostream>
#include"../WindowsCamTest/CellDetect.h"
#include <opencv2/videoio.hpp>
#include <opencv2/opencv.hpp>
//#include<opencv2/opencv.hpp>
#include "opencv2/highgui.hpp"
#include <string>
int main() {
	CellDetect celldetect;
	std::cout << "test" << std::endl;
	std::string imgPath = "E:\\Data\\CameraImages\\20220322\\0\\2022_03_22_160655152.bmp";
	cv::Mat img = cv::imread(imgPath);
	//cv::imshow("img",img);
	//waitKey(0);

	auto res = celldetect.GetResult(img);
	for (int i = 0; i < res.size(); i++) {
		cv::imshow("img",res[i].m_image);
		waitKey(0);
	}

	//std::cin.get();
}
