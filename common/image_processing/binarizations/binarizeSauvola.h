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

#ifndef PRLIB_binarizeSauvola_h
#define PRLIB_binarizeSauvola_h

#include <opencv2/core/core.hpp>

namespace prl
{

/*!
* \brief Sauvola binarization algorithm implementation.
* \param imageInput Image for processing.
* \param outputImage Resulting binary image.
* \param windowSize Size of sliding window.
* \param thresholdCoefficient Coefficient for threshold calculation.
* \param morphIterationCount Count of morphology operation in postprocessing.
* \details This function implements algorithm described in article
* "Efficient Implementation of Local Adaptive Thresholding Techniques Using Integral Images".
*/
CV_EXPORTS void binarizeSauvola(
		cv::Mat& imageInput, cv::Mat& outputImage,
		int windowSize = 101,
		double thresholdCoefficient = 0.01,
		int morphIterationCount = 2);
}
#endif // PRLIB_binarizeSauvola_h
