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

    std::vector<cv::Mat> CellDetect_ROI(std::vector<cv::Point2f> centroid, cv::Mat& src)
    {
        std::vector<cv::Mat> res;
        if (centroid.size() > 0)
            for (size_t k = 0; k < centroid.size(); ++k)
            {
                cv::Mat roiImg = src(Rect(centroid[k].x - width / 2, centroid[k].y - height / 2, width, height));
                res.push_back(roiImg);
            }
        return res;
    }

    std::vector <cv::Point2f>  CellDetect_searchCentroid(cv::Mat& src)
    {
        cv::Mat srcc = src;
        std::vector<cv::Point2f> Centroid;
        std::vector<std::vector<Point> > contours_big_img;
        vector<Vec4i> hierarchys;
        findContours(src, contours_big_img, hierarchys,
            cv::RETR_EXTERNAL, cv::CHAIN_APPROX_SIMPLE, Point(0, 0));  //寻找轮廓
        for (unsigned int i = 0; i < contours_big_img.size(); ++i)
        {
            float a = contourArea(contours_big_img[i]);
            if (a < 300 || a>100000)
                continue;
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

            Centroid.push_back(rectPoint.center);

        }
        return  Centroid;
    }
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
            cv::RETR_EXTERNAL, cv::CHAIN_APPROX_SIMPLE, Point(0, 0));  //寻找轮廓
        for (unsigned int i = 0; i < contours_big_img.size(); ++i)
        {
            float cell_area = contourArea(contours_big_img[i]) * UM_2_PER_PIXEL;          //求细胞面积

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
            float cell_peri = arcLength(contours_big_img[i], true) * UM_PER_PIXEL;     //求细胞周长

            if (cell_peri > 50.0 || cell_peri < 6)
                continue;

            cv::Point2f center;
            float cell_dia = 0.0;
            float cell_short_axis = 0.0;
            float cell_long_axis = 0.0;
            double cell_vol = 0.0;
            double cell_eccentricity = 0.0;
            RotatedRect ell;
            ell = minAreaRect(contours_big_img[i]);
            //ell = fitEllipse(contours_big_img[i]);  //椭圆拟合
            cell_short_axis = MIN(ell.size.height, ell.size.width) ;
            cell_long_axis = MAX(ell.size.height, ell.size.width);
            cell_dia = 2*sqrt(cell_area/pi);                //等效直径
            //cell_dia = UM_PER_PIXEL*(cell_short_axis+ cell_long_axis)/2;               
            cell_vol = pow(cell_dia, 3) *pi/6;    //等效体积
            cell_eccentricity = sqrt(pow(cell_long_axis, 2) - pow(cell_short_axis, 2)) / cell_long_axis;   //根据长短轴求偏心率

            res.push_back({ roiImg, cell_area, cell_peri, cell_dia ,cell_short_axis * UM_PER_PIXEL,cell_long_axis * UM_PER_PIXEL,cell_vol,cell_eccentricity });
        }
        return res;


        }
        //std::vector<cv::Mat> getResult(cv::Mat src)     //分割细胞图片
        //{
        //    if (!src.data)
        //        std::cerr << "Problem loading image!!!" << std::endl;
        //    cv::Mat gray, edge, src_1, src_2, src_c, src_4;

        //    resize(src, src_c, Size(0, 0), beta, beta, 4);

        //    boxFilter(src_c, src_1, -1, Size(3, 3));
        //    threshold(src_1, src_2, 125, 255, THRESH_BINARY);
        //    vector<vector<Point>>contours_big_img;
        //    findContours(255 - src_2, contours_big_img, RETR_EXTERNAL, CHAIN_APPROX_NONE);
        //    if (!contours_big_img.size())
        //        return std::vector<cv::Mat>();
        //    Mat src_3 = Mat::zeros(src_2.size(), CV_8U);
        //    drawContours(src_3, contours_big_img, -1, Scalar(255), 1);
        //    //imshow("src_3", src_3);
        //    // waitKey(0);
        //    //src_3 = 255 - src_3;
        //    resize(src_3, src_4, Size(320, 480));
        //    vector <Point2f> centroid = CellDetect_searchCentroid(src_4);
        //    vector <std::tuple<float, float, float>> cell_parms = Cell_computeparms(src_4);
        //    
        //    return CellDetect_ROI(centroid, src);
        //}

        //std::vector <std::tuple<float, float, float>> getparms(cv::Mat src)  //求细胞参数
        //{
        //    if (!src.data)
        //        std::cerr << "Problem loading image!!!" << std::endl;
        //    cv::Mat gray, edge, src_1, src_2, src_c, src_4;

        //    resize(src, src_c, Size(0, 0), beta, beta, 4);

        //    boxFilter(src_c, src_1, -1, Size(3, 3));
        //    threshold(src_1, src_2, 125, 255, THRESH_BINARY);
        //    vector<vector<Point>>contours_big_img;
        //    findContours(255 - src_2, contours_big_img, RETR_EXTERNAL, CHAIN_APPROX_NONE);
        //    if (!contours_big_img.size())
        //        return std::vector<std::tuple<float, float, float>>();
        //    Mat src_3 = Mat::zeros(src_2.size(), CV_8U);
        //    drawContours(src_3, contours_big_img, -1, Scalar(255), 1);
        //    resize(src_3, src_4, Size(320, 480));
        //    vector <std::tuple<float, float, float>> cell_parms = Cell_computeparms(src_4);
        //    return cell_parms;

        //}

        //std::vector <std::tuple< float, float, float>>  Cell_computeparms(cv::Mat& src)
        //{
        //    cv::Mat srcc = src;
        //    std::vector<std::vector<Point> > contours_big_img;
        //    vector<Vec4i> hierarchys;
        //    std::vector <std::tuple<float, float, float>> parm;

        //    findContours(src, contours_big_img, hierarchys,
        //        cv::RETR_EXTERNAL, cv::CHAIN_APPROX_SIMPLE, Point(0, 0));  //寻找轮廓
        //    for (unsigned int i = 0; i < contours_big_img.size(); ++i)
        //    {
        //        float cell_area = contourArea(contours_big_img[i]);          //求细胞面积
        //        if (cell_area < 300 || cell_area>100000)
        //            continue;
        //        float cell_peri = arcLength(contours_big_img[i], true);     //求细胞周长
        //        cv::Point2f center;
        //        float cell_rad;
        //        minEnclosingCircle(contours_big_img[i],center,cell_rad);     //求细胞最小外接圆直径
        //        parm.push_back(std::make_tuple(cell_area,cell_peri,cell_rad));
        //    }
        //    return parm;

        //}




};





