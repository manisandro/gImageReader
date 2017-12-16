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

#include "splitPage.h"
#include "deskew.h"
#include "imageLibCommon.h"

#include <algorithm>
#include <vector>

#include <opencv2/core/core.hpp>
#include <opencv2/imgproc/imgproc.hpp>

std::pair<cv::Point, cv::Point> findVertLine(const cv::Mat &inputImage) {

    cv::Mat imageToProc = inputImage.clone();
    if (inputImage.channels() == 3) {
        cv::cvtColor(inputImage, imageToProc, CV_BGR2GRAY);
    }
    // TODO: Should be possibly resized to smth like 256px

    // TODO: can be wrong in the reason of difficult for analyze page
    prl::deskew(inputImage, imageToProc);

    // TODO: check binarizeByVariance edge detection
    cv::Mat resultCanny;
    double upper = 50;
    double lower = 25;
    cv::Canny(imageToProc, resultCanny, lower, upper);

    // Use HoughLine detection for finding all lines
    std::vector<cv::Vec2f> lines, vertLines;
    const int thresholdHough = 50;
    cv::HoughLines(resultCanny, lines, 1, CV_PI / 180.0, thresholdHough, 0, 0);

    // Extract all vertical lines
    for (size_t i = 0; i < lines.size(); ++i)
    {
        float theta = lines[i][1];
        if (theta > CV_PI / 180.0 * 170.0 || theta < CV_PI / 180.0 * 10.0)
        {
            vertLines.push_back(lines[i]);
        }
    }

    // TODO: delete similar lines. Is it required?
    // deleteSimilarLines()

    // Find the nearest line to the center
    int center = resultCanny.cols / 2;
    for(const auto& line : vertLines)
    {
        auto lineByPoint = fromVec2f(line);

    }
};