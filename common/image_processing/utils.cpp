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

#include "utils.h"

#include <cmath>
#include <stdexcept>

#include <opencv2/core/core.hpp>

bool prl::eq_d(const double v1, const double v2, const double delta)
{
    return std::abs(v1 - v2) <= delta;
}

double prl::compareImages(const cv::Mat& image1, const cv::Mat& image2)
{
    cv::Mat image1Mat = image1.clone();
    cv::Mat image2Mat = image2.clone();

    if (image1Mat.empty())
    {
        throw std::invalid_argument("Input image #1 is empty");
    }

    if (image2Mat.empty())
    {
        throw std::invalid_argument("Input image #2 is empty");
    }

    bool isImageSizesEqual = image1Mat.size == image2Mat.size;
    bool isImageTypesEqual = image1Mat.type() == image2Mat.type();
    bool isImageChannelsCountsEqual = image1Mat.channels() == image2Mat.channels();
    if (!isImageSizesEqual ||
        !isImageTypesEqual ||
        !isImageChannelsCountsEqual)
    {
        return 0.0;
    }

    cv::Mat comparisonResultImage = (image1Mat == image2Mat); // -V601
    //comparisonResultImage *= 1.0f / 255.0f;
    comparisonResultImage &= cv::Scalar_<uchar>(1, 1, 1, 1);

    cv::Scalar comparisonResultImageChannelsSum = cv::sum(comparisonResultImage);
    comparisonResultImageChannelsSum /= static_cast<double>(image1Mat.total());

    double percent = 0.0;

    if (comparisonResultImage.channels() == 1)
    {
        percent = comparisonResultImageChannelsSum[0];
    }
    else
    {
        percent = 2.0f;

        for (int ch = 0; ch < comparisonResultImage.channels(); ++ch)
        {
            percent = std::min(comparisonResultImageChannelsSum[ch], percent);
        }
    }

    return percent;
}