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

#include "rotate.h"

#include <cmath>

#include <opencv2/imgproc/imgproc.hpp>

#include "utils.h"

namespace prl
{
void rotate(const cv::Mat& inputImage, cv::Mat& outputImage, double angle)
{
    angle = std::fmod(angle, 360.0);
    if (eq_d(angle, 90.0))
    {
        // rotate on 90
        cv::transpose(inputImage, outputImage);
        cv::flip(outputImage, outputImage, 1);
        return;
    }
    else if (eq_d(angle, 180.0))
    {
        // rotate on 180
        cv::flip(inputImage, outputImage, -1);
        return;
    }
    else if (eq_d(angle, 270.0))
    {
        // rotate on 270
        cv::transpose(inputImage, outputImage);
        cv::flip(outputImage, outputImage, 0);
        return;
    }
    else
    {
        outputImage = inputImage.clone();
        cv::bitwise_not(inputImage, inputImage);

        int len = std::max(outputImage.cols, outputImage.rows);
        cv::Point2f pt(static_cast<float>(len / 2.0), static_cast<float>(len / 2.0));
        cv::Mat r = cv::getRotationMatrix2D(pt, angle, 1.0);

        cv::warpAffine(inputImage, outputImage, r, cv::Size(len, len));

        cv::bitwise_not(inputImage, inputImage);
        cv::bitwise_not(outputImage, outputImage);
    }
}
}