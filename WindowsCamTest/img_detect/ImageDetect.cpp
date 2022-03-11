#include<cmath>
#include<direct.h> 
#include<io.h> 
#include<opencv2/opencv.hpp>
//#include "opencv2/highgui.hpp"
//#include "opencv2/ml.hpp"
//#include "opencv2/objdetect.hpp"
#include <opencv2/core/utils/logger.hpp>
#include<iostream>
#include<algorithm>
#include <stdio.h>
#include <opencv2/imgproc/imgproc_c.h>
#include <ctime>
using namespace std;
using namespace cv;

vector<Point2f>  searchConcavePoint(std::vector<std::vector<Point> >contours, Mat& src);
vector <Point2f>   searchCentroid(Mat& src);
void ROI(vector<Point2f> centroid, Mat& src, size_t i);
int width = 90, height = 100;
float beta=0.2;
String folder_rw = "E://cell_sample//15_new//";
String savename;
int main()
{
    
    if (0 != _access(folder_rw.c_str(), 0))
    {
        
        _mkdir(folder_rw.c_str());   
    }
    
    
    const char* nam = "scr_c";
    utils::logging::setLogLevel(utils::logging::LOG_LEVEL_SILENT);
    std::vector<cv::String> filenames; // notice here that we are using the Opencv's embedded "String" class
    
    String folder = "E://cell_sample//15//15//"; // again we are using the Opencv's embedded "String" class
    glob(folder, filenames); // new function that does the job ;-)
    clock_t startTime, endTime;
    startTime = clock();//计时开始
    //namedWindow("scra", 0);
    for (size_t i = 0; i < filenames.size(); ++i)
    {
        cout << filenames[i] << endl;
        Mat src = imread(filenames[i], IMREAD_GRAYSCALE), gray, edge, src_1, src_2,src_c, src_4;
        
        if (!src.data)
            std::cerr << "Problem loading image!!!" << std::endl;
      // imshow("src", src);
       
       

       resize(src, src_c, Size(0, 0),beta,beta,4);
       
       boxFilter(src_c, src_1, -1, Size(7, 9));
       //namedWindow(nam, 0);
       //resizeWindow(nam, Size(480, 640));
       // imshow(nam, src_c);
       
        //imshow("src_1", src_1);
        threshold(src_1, src_2, 125, 255, THRESH_BINARY);
        //imshow("src_2", src_2);
       
        vector<vector<Point>>contours;
        findContours(255 - src_2, contours, RETR_EXTERNAL, CHAIN_APPROX_NONE);
        if (!contours.size())
            continue;
        Mat src_3 = Mat::zeros(src_2.size(), CV_8U);
        drawContours(src_3, contours, -1, Scalar(255), 1);
        //imshow("src_3", src_3);
        // waitKey(0);
        //src_3 = 255 - src_3;
        resize(src_3, src_4, Size(320, 480));
        vector <Point2f> centroid=searchCentroid(src_4);
        ROI(centroid, src,i);
    }
   
    endTime = clock();//计时结束
    cout << "The run time is: " << (double)(endTime - startTime) / CLOCKS_PER_SEC << "s" << endl;
    return 0;
}
void ROI(vector<Point2f> centroid, Mat& src, size_t i) {

    if (centroid.size() > 0)
        for (size_t k = 0; k < centroid.size(); ++k)
        {
            Mat roiImg = src(Rect(centroid[k].x - width / 2, centroid[k].y - height / 2, width, height));
            savename = folder_rw + to_string (i+1)+'_'+ to_string(k+1)+".bmp";
           
            imwrite(savename, roiImg);
            // imshow("ROI", roiImg);
             //waitKey(0);   
        }
}

vector <Point2f>  searchCentroid(Mat& src)
{
    Mat srcc = src;
    vector<Point2f> Centroid;
    vector<vector<Point> > contours;
    vector<Vec4i> hierarchys;
    findContours(src, contours, hierarchys,
        CV_RETR_EXTERNAL, CV_CHAIN_APPROX_SIMPLE, Point(0, 0));  //寻找轮廓
    for (unsigned int i = 0; i < contours.size(); ++i)
    {
        double a = contourArea(contours[i]);
        if(a<300||a>100000)
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