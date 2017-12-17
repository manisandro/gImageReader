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

#include "removeDots.h"

#include <vector>

#include <opencv2/core/core.hpp>
#include <opencv2/features2d/features2d.hpp>
#include <opencv2/imgproc/imgproc.hpp>

void prl::removeDots(const cv::Mat& inputImage, cv::Mat& outputImage, const double radius)
{
    outputImage = inputImage.clone();

    size_t maxDotDiameter = inputImage.cols * 0.01;

    // Converting to grayscale
    cv::Mat gray;
    if (inputImage.channels() == 3)
    {
        cv::cvtColor(inputImage, gray, CV_BGR2GRAY);
    }
    else
    {
        gray = inputImage;
    }

    // Configure blob detector
    cv::SimpleBlobDetector::Params params;

    params.filterByArea = true;
    // TODO: calculate coefficient depend on image size
    params.maxArea = CV_PI * maxDotDiameter * maxDotDiameter / 4.0;
    params.minArea = 0;

    params.filterByCircularity = true;
    params.minCircularity = 0.9;
    //params.maxCircularity = 1.0;

    cv::Ptr<cv::SimpleBlobDetector> detector = cv::SimpleBlobDetector::create(params);

    // Detect blobs.
    std::vector<cv::KeyPoint> keypoints;
    detector->detect(gray, keypoints);

    // Border heuristic
    int width = inputImage.cols;
    int height = inputImage.rows;

    int leftX = width * 0.1;
    int rightX = width - leftX;
    int upY = height * 0.1;
    int bottomY = height - upY;

    std::vector<cv::KeyPoint> filteredKeypoints = keypoints;
    /*for(const auto& keypoint : keypoints)
    {
        if(keypoint.pt.x > leftX && keypoint.pt.x < rightX &&
           keypoint.pt.y > upY && keypoint.pt.y < bottomY)
        {
            continue;
        }
        filteredKeypoints.push_back(keypoint);
    }*/
    // End border heuristic

    // Fill keypoits by white color.
    // TODO: maybe we should find automatically background color and use it for filling instead of white
    /*for(const auto& keypoint : filteredKeypoints)
    {
        cv::circle(outputImage, keypoint.pt, keypoint.size / 2.0 + 3, cv::Scalar(255,255,255), CV_FILLED);
    }*/
    cv::drawKeypoints(outputImage, keypoints, outputImage, cv::Scalar(0,0,255), cv::DrawMatchesFlags::DRAW_RICH_KEYPOINTS);
}