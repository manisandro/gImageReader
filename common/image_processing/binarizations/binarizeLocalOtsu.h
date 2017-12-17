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

#ifndef PRLIB_binarizeLocalOtsu_h
#define PRLIB_binarizeLocalOtsu_h

#include <opencv2/core/core.hpp>

namespace prl
{

/*!
 * \brief Implementation of local Otsu binarization algorithm.
 * \param inputImage Image for binarization.
 * \param outputImage Resulting image.
 * \param maxValue New value for pixel which intensity greater than threshold value.
 * \param CLAHEClipLimit Parameter for CLAHE procedure.
 * \param GaussianBlurKernelSize Parameter for Gaussian blur procedure.
 * \param CannyUpperThresholdCoeff Coefficient for upper threshold of Canny edge detector.
 * \param CannyLowerThresholdCoeff Coefficient for lower threshold of Canny edge detector.
 * \param CannyMorphIters Parameter of morphology operations.
 * \details This function consists of next steps:
 * -# Histogram enchanting by CLAHE.
 * -# Edges detection by Canny.
 * -# Morphology operations.
 * -# Contours detection.
 * -# Binarization by Otsu of bounding rectangle of each contour.
 */
CV_EXPORTS void binarizeLocalOtsu(
		cv::Mat& inputImage, cv::Mat& outputImage,
		double maxValue = 255.0,
		double CLAHEClipLimit = 0.0,
		int GaussianBlurKernelSize = 19,
		double CannyUpperThresholdCoeff = 0.15,
		double CannyLowerThresholdCoeff = 0.01,
		int CannyMorphIters = 1);

}

#endif // PRLIB_binarizeLocalOtsu_h
