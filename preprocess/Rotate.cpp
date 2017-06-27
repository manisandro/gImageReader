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

#include "Rotate.hpp"

#include "opencv2/imgproc.hpp"

namespace prl
{
void rotate(const cv::Mat& input, cv::Mat& output, const double angle)
{
    if (angle == 90)
    {
        // rotate on 90
        cv::transpose(input, output);
        cv::flip(output, output, 1);
        return;
    }
    else if (angle == 180)
    {
        // rotate on 180
        cv::flip(input, output, -1);
        return;
    }
    else if (angle == 270)
    {
        // rotate on 270
        cv::transpose(input, output);
        cv::flip(output, output, 0);
        return;
    }
    else
    {
        output = input.clone();
        cv::bitwise_not(input, input);

        int len = std::max(output.cols, output.rows);
        cv::Point2f pt(static_cast<float>(len / 2.0), static_cast<float>(len / 2.0));
        cv::Mat r = cv::getRotationMatrix2D(pt, angle, 1.0);

        cv::warpAffine(input, output, r, cv::Size(len, len));

        cv::bitwise_not(input, input);
        cv::bitwise_not(output, output);
    }
}
}