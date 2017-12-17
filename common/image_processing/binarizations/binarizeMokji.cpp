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

#include <array>
#include <stdexcept>

#include <opencv2/core/core.hpp>
#include <opencv2/imgproc/imgproc.hpp>
#include <opencv2/highgui/highgui.hpp>

namespace prl
{

void binarizeMokji(const cv::Mat& inputImage, cv::Mat& outputImage,
                   size_t maxEdgeWidth, size_t minEdgeMagnitude)
{
    if (maxEdgeWidth < 1)
    {
        throw std::invalid_argument("mokjiThreshold: invalud maxEdgeWidth");
    }
    if (minEdgeMagnitude < 1)
    {
        throw std::invalid_argument("mokjiThreshold: invalid minEdgeMagnitude");
    }

    cv::Mat gray;
    cv::cvtColor(inputImage, gray, CV_BGR2GRAY);

    const size_t dilateSize = (maxEdgeWidth + 1) * 2 - 1;

    cv::Mat dilatedImage;
    cv::dilate(gray, dilatedImage, cv::getStructuringElement(cv::MORPH_RECT, cv::Size(dilateSize, dilateSize)));

    std::array<std::array<size_t, 256>, 256> matrix;

    const int w = inputImage.cols;
    const int h = inputImage.rows;

    for (int y = maxEdgeWidth; y < h - static_cast<int>(maxEdgeWidth); ++y)
    {
        for (int x = maxEdgeWidth; x < w - static_cast<int>(maxEdgeWidth); ++x)
        {
            const size_t pixel = gray.at<uchar>(cv::Point(x, y));
            const size_t darkestNeighbour = dilatedImage.at<uchar>(cv::Point(x, y));

            assert(darkestNeighbour >= pixel);

            ++matrix[darkestNeighbour][pixel];
        }
    }

    size_t nominator = 0;
    size_t denominator = 0;
    for (size_t m = 0; m < 256 - minEdgeMagnitude; ++m)
    {
        for (size_t n = m + minEdgeMagnitude; n < 256; ++n)
        {
            assert(n >= m);

            const size_t val = matrix[n][m];
            nominator += (m + n) * val;
            denominator += val;
        }
    }

    if (denominator == 0)
    {
        cv::threshold(gray, outputImage, 128, 255, CV_THRESH_BINARY);
    }

    const int threshold = 0.5 * nominator / denominator + 0.5;
    cv::threshold(gray, outputImage, threshold, 255, CV_THRESH_BINARY);
}

}

