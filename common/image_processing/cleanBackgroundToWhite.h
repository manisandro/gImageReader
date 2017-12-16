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

#ifndef PRLIB_CLEANBACKGROUNDTOWHITE_H
#define PRLIB_CLEANBACKGROUNDTOWHITE_H

#include <opencv2/core/core.hpp>

namespace prl
{
/*!
 * \brief Clean image background to white color.
 * \param[in] inputImage Input image.
 * \param[out] outputImage Output image.
 * \details Implementation is based on Leptonica library
 * \note Implementation of this procedure is based on
 * <a href="http://www.leptonica.com/">Leptonica library</a>..
 */
    CV_EXPORTS void cleanBackgroundToWhite(const cv::Mat& inputImage, cv::Mat& outputImage);
}

#endif //PRLIB_CLEANBACKGROUNDTOWHITE_H
