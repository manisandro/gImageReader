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

#ifndef PRLIB_WARP
#define PRLIB_WARP

#include <opencv2/core/core.hpp>

namespace prl
{

/*!
 * \brief Transform image to crop document.
 * \param[in] inputImage Input image.
 * \param[out] outputImage Output image.
 * \param[in] x0 Top left x coordinate.
 * \param[in] y0 Top left y coordinate.
 * \param[in] x1 Top right x coordinate.
 * \param[in] y1 Top right y coordinate.
 * \param[in] x2 Bottom right x coordinate.
 * \param[in] y2 Bottom right y coordinate.
 * \param[in] x3 Bottom left x coordinate.
 * \param[in] y3 Bottom left y coordinate.
 * \param[in] ratio Expected Height/Width ratio
 * \details Crop image and do warp transformation to rectangle.
 * Calculates aspect ratio of source image and keeps it after crop.
 */
CV_EXPORTS void warpCrop(const cv::Mat& inputImage,
              cv::Mat& outputImage,
              const int x0, const int y0,
              const int x1, const int y1,
              const int x2, const int y2,
              const int x3, const int y3,
              double ratio = -1.0,
              const int borderMode = cv::BORDER_CONSTANT,
              const cv::Scalar& borderValue = cv::Scalar());


/*!
 * \brief Transform image to crop document.
 * \param[in] inputImage Input image.
 * \param[out] outputImage Output image.
 * \param[in] points Document contour.
 * \param[in] ratio Expected Height/Width ratio
 * \details Crop image and do warp transformation to rectangle.
 * Calculates aspect ratio of source image and keeps it after crop.
 */
CV_EXPORTS void warpCrop(cv::Mat& inputImage, cv::Mat& outputImage,
              const std::vector<cv::Point>& points,
              double ratio = -1.0,
              int borderMode = cv::BORDER_CONSTANT,
              const cv::Scalar& borderValue = cv::Scalar());
}

#endif //PRLIB_WARP
