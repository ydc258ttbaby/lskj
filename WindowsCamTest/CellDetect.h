#pragma once
#include<cmath>
#include<direct.h> 
#include<io.h> 

#include <opencv2/videoio.hpp>
#include <opencv2/opencv.hpp>
//#include<opencv2/opencv.hpp>
#include "opencv2/highgui.hpp"
//#include "opencv2/ml.hpp"
//#include "opencv2/objdetect.hpp"
//#include <opencv2/core/utils/logger.hpp>
#include<iostream>
#include<algorithm>
#include <stdio.h>
#include <ctime>
#include <vector>


using namespace std;
using namespace cv;
class CellDetect
{
private :
    int width = 100, height = 100;
    float beta = 0.2;
public:

    std::vector<cv::Mat> CellDetect_ROI(std::vector<cv::Point2f> centroid, cv::Mat& src, size_t i) {
        std::vector<cv::Mat> res;
        if (centroid.size() > 0)
            for (size_t k = 0; k < centroid.size(); ++k)
            {
                cv::Mat roiImg = src(Rect(centroid[k].x - width / 2, centroid[k].y - height / 2, width, height));
                res.push_back(roiImg);
            }
        return res;
    }

    std::vector <cv::Point2f>   CellDetect_searchCentroid(cv::Mat& src)
    {
        cv::Mat srcc = src;
        std::vector<cv::Point2f> Centroid;
        std::vector<std::vector<Point> > contours;
        vector<Vec4i> hierarchys;
        findContours(src, contours, hierarchys,
            cv::RETR_EXTERNAL, cv::CHAIN_APPROX_SIMPLE, Point(0, 0));  //Ñ°ÕÒÂÖÀª
        for (unsigned int i = 0; i < contours.size(); ++i)
        {
            double a = contourArea(contours[i]);
            if (a < 300 || a>100000)
                continue;
            RotatedRect rectPoint = minAreaRect(contours[i]);
            if (rectPoint.center.x - width / 2 <= 0)
                rectPoint.center.x = width / 2 + 1;
            if (rectPoint.center.x + width / 2 > 319)
                rectPoint.center.x = 319 - width / 2;
            if (rectPoint.center.y - height / 2 <= 0)
                rectPoint.center.y = height / 2 + 1;
            if (rectPoint.center.y + height / 2 > 479)
                rectPoint.center.y = 479 - height / 2;

            Centroid.push_back(rectPoint.center);

        }
        return  Centroid;
    }

    std::vector<cv::Mat> getResult(cv::Mat src) {
        if (!src.data)
            std::cerr << "Problem loading image!!!" << std::endl;
        cv::Mat gray, edge, src_1, src_2, src_c, src_4;

        resize(src, src_c, Size(0, 0), beta, beta, 4);

        boxFilter(src_c, src_1, -1, Size(7, 9));
        threshold(src_1, src_2, 125, 255, THRESH_BINARY);

        vector<vector<Point>>contours;
        findContours(255 - src_2, contours, RETR_EXTERNAL, CHAIN_APPROX_NONE);
        if (!contours.size())
            return std::vector<cv::Mat>();
        Mat src_3 = Mat::zeros(src_2.size(), CV_8U);
        drawContours(src_3, contours, -1, Scalar(255), 1);
        //imshow("src_3", src_3);
        // waitKey(0);
        //src_3 = 255 - src_3;
        resize(src_3, src_4, Size(320, 480));
        vector <Point2f> centroid = CellDetect_searchCentroid(src_4);
        int i = 0;
        return CellDetect_ROI(centroid, src, i);
    }
};






