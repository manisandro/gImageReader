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

#include "resize.h"

#include <opencv2/imgproc/imgproc.hpp>

void prl::resize(const cv::Mat& src, cv::Mat& dst, int scaleX, int scaleY, int maxSize)
{
    cv::Size newImageSize;

    if (scaleX > 0 && scaleY > 0)
    {
        newImageSize = cv::Size(
                static_cast<int>(src.cols * scaleX),
                static_cast<int>(src.rows * scaleY)
        );

        cv::resize(src, dst, newImageSize, 0, 0, cv::INTER_AREA);
    }
    else
    {
        int longSide = std::max(src.cols, src.rows);

        int scaleFactorX = 1;
        int scaleFactorY = 1;

        dst = src.clone();

        while (longSide > maxSize)
        {
            cv::pyrDown(dst, dst);

            longSide = std::max(dst.cols, dst.rows);
            scaleFactorX *= 2;
            scaleFactorY *= 2;
        }

        newImageSize = cv::Size(src.cols / scaleFactorX, src.rows / scaleFactorY);
    }
}