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

#ifndef PRLIB_COCOCLUST_Binarizator_h
#define PRLIB_COCOCLUST_Binarizator_h

#include <opencv2/core/core.hpp>


namespace prl
{

/*!
 * \brief binarizeCOCOCLUST
 * \param[in] inputImage Input image.
 * \param[out] outputImage Output image.
 * \param[in] T_S Threshold for preliminary clustering.
 * \param[in] CLAHEClipLimit \parblock
 * Parameter of local contrast enhancement procedure
 * (if less or equal than 0 then procedure is not used).
 * \endparblock
 * \param[in] GaussianBlurKernelSize kernel size of Gaussian blur procedure.
 * \param[in] CannyUpperThresholdCoeff Coefficient for upper threshold of Canny edge detector.
 * \param[in] CannyLowerThresholdCoeff Coefficient for lower threshold of Canny edge detector.
 * \param[in] CannyMorphIters \parblock
 * Parameter of erode and dilatation (if equal than 0 then procedures are not used).
 * \endparblock
 */
CV_EXPORTS void binarizeCOCOCLUST(cv::Mat& inputImage, cv::Mat& outputImage,
                       const float T_S = 45,
                       double CLAHEClipLimit = 3.0,
                       int GaussianBlurKernelSize = 19,
                       double CannyUpperThresholdCoeff = 0.15,
                       double CannyLowerThresholdCoeff = 0.05,
                       int CannyMorphIters = 4);
}
#endif // PRLIB_COCOCLUST_Binarizator_h
