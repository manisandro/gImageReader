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

#include "basicDeblur.h"

#include <stdexcept>
#include <vector>

#include <opencv2/core/core.hpp>
#include <opencv2/imgproc/imgproc.hpp>

void prl::basicDeblur(const cv::Mat& inputImage, cv::Mat& outputImage,
                      const size_t gaussianKernelSize, const double sigmaX, const double sigmaY,
                      const double imageWeight)
{
    cv::Mat inputImageMat = inputImage;

    if (inputImageMat.empty())
    {
        throw std::invalid_argument("Input image for deblurring is empty");
    }

    cv::Mat outputImageMat;

    std::vector<cv::Mat> channels;
    cv::split(inputImageMat, channels);

    for (cv::Mat& channel : channels)
    {
        cv::Mat channelfloat;
        channel.convertTo(channelfloat, CV_32F);
        cv::GaussianBlur(
                channelfloat, channel,
                cv::Size(gaussianKernelSize, gaussianKernelSize),
                sigmaX, sigmaY);
        cv::addWeighted(
                channelfloat, 2.0 * imageWeight,
                channel, 2.0 * imageWeight - 2.0,
                0.0,
                channel);

        // convert back to 8bits gray scale
        channel.convertTo(channel, CV_8U);
    }

    cv::merge(channels, outputImageMat);

    outputImage = outputImageMat;
}
