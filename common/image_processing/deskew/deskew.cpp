/*
    MIT License

    Copyright (c) 2017 Alexander Zaitsev

    Permission is hereby granted, free of charge, to any person obtaining a copy
    of this software and associated documentation files (the "Software"), to deal
    in the Software without restriction, including without limitation the rights
    to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
    copies of the Software, and to permit persons to whom the Software is
    furnished to do so, subject to the following conditions:

    The above copyright notice and this permission notice shall be included in all
    copies or substantial portions of the Software.

    THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
    IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
    FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
    AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
    LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
    OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
    SOFTWARE.
*/

#include "deskew.h"

#include <opencv2/imgproc/imgproc.hpp>
#include <opencv2/highgui/highgui.hpp>

#if defined(WIN32) || defined(_MSC_VER)
#include <windows.h>
#else
#include "compatibility.h"
#endif

#include <leptonica/allheaders.h>


#if defined(_WIN32) || defined(_MSC_VER)
#include <direct.h>
#include <codecvt>
#else

#include <unistd.h>

#endif //WIN32

#include <algorithm>
#include <functional>
#include <list>
#include <map>
#include <vector>

#include "formatConvert.h"

#ifndef M_PI

#ifdef CV_PI
#define M_PI CV_PI
#else
#define M_PI 3.141592653
#endif

#endif // !M_PI

#include "formatConvert.h"
#include "rotate.h"
#include "utils.h"

double prl::findOrientation(const cv::Mat& inputImage)
{
    cv::Mat grayImage;
    grayImage = inputImage.clone();
    if(inputImage.channels() == 3)
    {
        cv::cvtColor(inputImage, grayImage, CV_BGR2GRAY);
    }

    cv::adaptiveThreshold(grayImage, grayImage, 255, CV_ADAPTIVE_THRESH_MEAN_C, CV_THRESH_BINARY, 19, 9);

    PIX* pix = prl::opencvToLeptonica(&grayImage);
    if (!pix)
    {
        return 0;
    }

    l_int32 iOrientation = 0;
    {
        l_float32 fUpConf;
        l_float32 fLeftConf;
        if (pixOrientDetectDwa(pix, &fUpConf, &fLeftConf, 0, 0) != 0)
        {
            if (pix)
            {
                pixDestroy(&pix);
            }

            return 0;
        }

        if (makeOrientDecision(fUpConf, fLeftConf, 0.0, 0.0, &iOrientation, 0) != 0)
        {
            if (pix)
            {
                pixDestroy(&pix);
            }

            return 0;
        }
    }

    double angle = 0;
    if (iOrientation == L_TEXT_ORIENT_UP)
    {
        angle = 0.0;
    }
    else if (iOrientation == L_TEXT_ORIENT_LEFT)
    {
        angle = 90.0;
    }
    else if (iOrientation == L_TEXT_ORIENT_DOWN)
    {
        angle = 180.0;
    }
    else if (iOrientation == L_TEXT_ORIENT_RIGHT)
    {
        angle = 270.0;
    }
    else // if (iOrientation == L_TEXT_ORIENT_UNKNOWN)
    {
        angle = 0.0;
    }

    pixDestroy(&pix);

    return angle;
}


double prl::findAngle(const cv::Mat& inputImage)
{
    // AA: we need black-&-white image here even if none of threshold algorithms were called before
    //cv::adaptiveThreshold(input, input, 255, CV_ADAPTIVE_THRESH_GAUSSIAN_C, CV_THRESH_BINARY, 15, 5);
    cv::Mat input = inputImage.clone();

    cv::Size imgSize = input.size();
    cv::bitwise_not(input, input);
    std::vector<cv::Vec4i> lines;
    cv::HoughLinesP(input, lines, 1, CV_PI / 180, 100, imgSize.width / 8.f, 20);
    cv::Mat disp_lines(imgSize, CV_8UC1, cv::Scalar(0, 0, 0));

    const int nb_lines = static_cast<int>(lines.size());
    if (!nb_lines)
    {
        return 0.0;
    }

    std::vector<double> cv_angles = std::vector<double>(nb_lines);

    for (int i = 0; i < nb_lines; ++i)
    {
        cv::line(
                disp_lines,
                cv::Point(lines[i][0], lines[i][1]),
                cv::Point(lines[i][2], lines[i][3]),
                cv::Scalar(255, 0, 0));

        cv_angles[i] = atan2(
                (double) lines[i][3] - lines[i][1],
                (double) lines[i][2] - lines[i][0]);
    }

    const double delta = 0.01; //difference is less than 1 deg.
    std::list<std::pair<double, int> > t_diff;

    for (auto it = cv_angles.begin(); it != cv_angles.end(); ++it)
    {
        bool found = false;
        // bypass list
        for (auto elem = t_diff.begin(); elem != t_diff.end(); ++elem)
        {
            if (prl::eq_d(*it, elem->first, delta))
            {
                elem->second++;
                found = true;
                break;
            }
        }
        if (!found)
        {
            std::pair<double, int> p(*it, 0);
            t_diff.push_back(p);
        }
    }

    std::pair<double, int> max_elem =
            *(std::max_element(t_diff.begin(), t_diff.end(),
                               [](const std::pair<double, int>& v1,
                                  const std::pair<double, int>& v2)
                               { return v1.second < v2.second; }));

    const double cv_angle = max_elem.first * 180 / M_PI;

    cv::bitwise_not(input, input);

    return cv_angle;
}

CV_EXPORTS bool prl::deskew(const cv::Mat& inputImage, cv::Mat& outputImage)
{
    CV_Assert(!inputImage.empty());

    cv::Mat processingImage;

    if (inputImage.channels() != 1)
    {
        cv::cvtColor(inputImage, processingImage, CV_BGR2GRAY);
    }
    else
    {
        processingImage = inputImage.clone();
    }

    //TODO: Should we use here another binarization algorithm?
    cv::threshold(processingImage, processingImage, 128, 255, CV_THRESH_BINARY | CV_THRESH_OTSU);

    double angle = findAngle(processingImage);

    if ((angle != 0) && (angle <= DBL_MAX && angle >= -DBL_MAX))
    {
        prl::rotate(inputImage, outputImage, angle);
        prl::rotate(processingImage, processingImage, angle);
    }
    else
    {
        outputImage = inputImage.clone();
    }

    angle = findOrientation(processingImage);

    if (angle != 0.0)
    {
        prl::rotate(outputImage, outputImage, angle);
    }

    if (outputImage.empty())
    {
        return false;
    }

    return true;
}