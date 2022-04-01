#pragma once
#include<cmath>
#include<direct.h> 
#include<io.h> 
#include<math.h>

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


#define UM_PER_PIXEL 0.13824
#define UM_2_PER_PIXEL 0.01911
#define pi 3.1415926

using namespace std;
using namespace cv;

struct CellInfo {
    cv::Mat m_image;
    double m_area;
    double m_perimeter;
    double m_diameter;
    double m_shortaxis;
    double m_longaxis;
    double m_vol;
    double m_eccentricity;
    double m_roundness;
    int m_id;
    int m_second;
    std::string m_name;
    std::string m_path;
    int m_class;
};

struct ImageInfo {
    cv::Mat m_image;
    int m_id;
    int m_second;
    std::string m_name;
};
class CellDetect
{
private:
    int width = 100, height = 100;
    int img_w = 320, img_h = 480;
    float beta = 0.5;
public:



  
    std::vector<CellInfo> GetResult(cv::Mat src) {
        std::vector<CellInfo> res;

        if (!src.data)
            std::cerr << "Problem loading image!!!" << std::endl;
        cv::Mat gray, edge, src_1, src_2, src_c, src_4;

        resize(src, src_c, Size(0, 0), beta, beta, 4);

        boxFilter(src_c, src_1, -1, Size(3, 3));
        threshold(src_1, src_2, 125, 255, THRESH_BINARY);
        //imshow("src_3", src_2);
        //waitKey(0);
        vector<vector<Point>> contours_small_img;
        findContours(255 - src_2, contours_small_img, RETR_EXTERNAL, CHAIN_APPROX_NONE);
        if (!contours_small_img.size())
            return res;
        Mat src_3 = Mat::zeros(src_2.size(), CV_8U);
        drawContours(src_3, contours_small_img, -1, Scalar(255), 1);

        //src_3 = 255 - src_3;
        resize(src_3, src_4, Size(img_w, img_h));

        std::vector<std::vector<Point> > contours_big_img;
        vector<Vec4i> hierarchys;
        findContours(src_4, contours_big_img, hierarchys,
            cv::RETR_EXTERNAL, cv::CHAIN_APPROX_SIMPLE, Point(0, 0));  //Ѱ������
        for (unsigned int i = 0; i < contours_big_img.size(); ++i)
        {
            float cell_area = contourArea(contours_big_img[i]) * UM_2_PER_PIXEL;          //��ϸ�����

            RotatedRect rectPoint = minAreaRect(contours_big_img[i]);

            if (rectPoint.center.x - width / 2 <= 0 || rectPoint.center.x + width / 2 > img_w - 1 || rectPoint.center.y - height / 2 <= 0 || rectPoint.center.y + height / 2 > img_h - 1)
                continue;

            /*if (rectPoint.center.x - width / 2 <= 0)
                rectPoint.center.x = width / 2 + 1;
            if (rectPoint.center.x + width / 2 > img_w - 1)
                rectPoint.center.x = img_w - 1 - width / 2;
            if (rectPoint.center.y - height / 2 <= 0)
                rectPoint.center.y = height / 2 + 1;
            if (rectPoint.center.y + height / 2 > img_h - 1)
                rectPoint.center.y = img_h - 1 - height / 2;*/

            cv::Mat roiImg = src(Rect(rectPoint.center.x - width / 2, rectPoint.center.y - height / 2, width, height));
            float cell_peri = arcLength(contours_big_img[i], true) * UM_PER_PIXEL;     //��ϸ���ܳ�

            if (cell_peri > 50.0 || cell_peri < 6)
                continue;

            cv::Point2f center;
            float cell_dia = 0.0;
            float cell_short_axis = 0.0;
            float cell_long_axis = 0.0;
            double cell_vol = 0.0;
            double cell_eccentricity = 0.0;
            double cell_roundness = 0.0;
            RotatedRect ell;
            ell = minAreaRect(contours_big_img[i]);
            //ell = fitEllipse(contours_big_img[i]);  //��Բ���
            cell_short_axis = MIN(ell.size.height, ell.size.width) ;
            cell_long_axis = MAX(ell.size.height, ell.size.width);
            cell_dia = 2*sqrt(cell_area/pi);                //��Чֱ��
            //cell_dia = UM_PER_PIXEL*(cell_short_axis+ cell_long_axis)/2;               
            cell_vol = pow(cell_dia, 3) *pi/6;    //��Ч���
            cell_eccentricity = (cell_long_axis- cell_short_axis) / cell_long_axis;   //���ݳ�������ƫ����

            cell_roundness = cell_area/ pow(cell_peri, 2)*4*pi;


            res.push_back({ roiImg, cell_area, cell_peri, cell_dia ,cell_short_axis * UM_PER_PIXEL,cell_long_axis * UM_PER_PIXEL,cell_vol,cell_eccentricity, cell_roundness });
        }
        return res;


        }
        



};





