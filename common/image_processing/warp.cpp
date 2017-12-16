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

#include "warp.h"

#include <algorithm>
#include <cmath>

#include <opencv2/imgproc/imgproc.hpp>

void prl::warpCrop(const cv::Mat& inputImage,
                   cv::Mat& outputImage,
                   const int x0, const int y0,
                   const int x1, const int y1,
                   const int x2, const int y2,
                   const int x3, const int y3,
                   double ratio,
                   const int borderMode /*= cv::BORDER_CONSTANT*/,
                   const cv::Scalar& borderValue /*= cv::Scalar()*/)
{
    const double side1 = std::sqrt((x1 - x0) * (x1 - x0) + (y1 - y0) * (y1 - y0));
    const double side2 = std::sqrt((x3 - x2) * (x3 - x2) + (y3 - y2) * (y3 - y2));
    const double side3 = std::sqrt((x3 - x0) * (x3 - x0) + (y3 - y0) * (y3 - y0));
    const double side4 = std::sqrt((x2 - x1) * (x2 - x1) + (y2 - y1) * (y2 - y1));

    long bitmapWidth = cvRound(std::max(side1, side2));
    long bitmapHeight = cvRound(std::max(side3, side4));

    if (ratio > 0.0)
    {
        bitmapWidth = cvRound(bitmapHeight / ratio);
    }

    float srcBuff[] = {static_cast<float>(x0), static_cast<float>(y0),
                       static_cast<float>(x1), static_cast<float>(y1),
                       static_cast<float>(x2), static_cast<float>(y2),
                       static_cast<float>(x3), static_cast<float>(y3)};

    float dstBuff[] = {static_cast<float>(0), static_cast<float>(0),
                       static_cast<float>(bitmapWidth), static_cast<float>(0),
                       static_cast<float>(bitmapWidth), static_cast<float>(bitmapHeight),
                       static_cast<float>(0), static_cast<float>(bitmapHeight)};

    cv::Mat src(4, 1, CV_32FC2, srcBuff);
    cv::Mat dst(4, 1, CV_32FC2, dstBuff);

    cv::Mat perspectiveTransform = cv::getPerspectiveTransform(src, dst);
    cv::warpPerspective(
            inputImage, outputImage, perspectiveTransform, cv::Size(bitmapWidth, bitmapHeight),
            cv::INTER_LINEAR,
            borderMode, borderValue);
}


void prl::warpCrop(cv::Mat& inputImage, cv::Mat& outputImage,
                   const std::vector<cv::Point>& points,
                   double ratio,
                   int borderMode /*= cv::BORDER_CONSTANT*/,
                   const cv::Scalar& borderValue /*= cv::Scalar()*/)
{
    if (inputImage.empty())
    {
        throw std::invalid_argument("Image for warping is empty");
    }

    if (points.size() != 4)
    {
        throw std::invalid_argument("Size of array of base points for warping isn't equal 4");
    }

    int x0 = cvRound(points[0].x);
    int y0 = cvRound(points[0].y);
    int x1 = cvRound(points[1].x);
    int y1 = cvRound(points[1].y);
    int x2 = cvRound(points[2].x);
    int y2 = cvRound(points[2].y);
    int x3 = cvRound(points[3].x);
    int y3 = cvRound(points[3].y);

    warpCrop(inputImage, outputImage, x0, y0, x1, y1, x2, y2, x3, y3, ratio, borderMode, borderValue);
}
