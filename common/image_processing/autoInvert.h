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

#ifndef AutoInvertFilter_OpenCV_h__
#define AutoInvertFilter_OpenCV_h__

#include <opencv2/core/core.hpp>

namespace prl
{
/*!
 * \brief Auto invert image.
 * \param[in] inputImage Input image.
 * \param[out] outputImage Output image.
 * \details See needAutoInvert for details.
 */
CV_EXPORTS void autoInvert(const cv::Mat& inputImage, cv::Mat& outputImage);

/*!
 * \brief Invert image.
 * \param[in] inputImage Input image.
 * \param[out] outputImage Output image.
 */
CV_EXPORTS void invert(const cv::Mat& inputImage, cv::Mat& outputImage);

/*!
 * \brief Checks is auto invertion needed.
 * \param[in] inputImage Input image.
 * \return True, if auto invertion required. Otherwise, false.
 * \details Calculates mean color and compares it with threshold.
 */
CV_EXPORTS bool needAutoInvert(const cv::Mat& inputImage);
}

#endif // AutoInvertFilter_OpenCV_h__

