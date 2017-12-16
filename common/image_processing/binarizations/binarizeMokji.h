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

#ifndef PRLIB_BINARIZEMOKJI_H
#define PRLIB_BINARIZEMOKJI_H

#include <opencv2/core/core.hpp>

namespace prl
{

/**
* \brief Image binarization using Mokji's global thresholding method.
*
* M. M. Mokji, S. A. R. Abu-Bakar: Adaptive Thresholding Based on
* Co-occurrence Matrix Edge Information. Asia International Conference on
* Modelling and Simulation 2007: 444-450
* http://www.academypublisher.com/jcp/vol02/no08/jcp02084452.pdf
*
* \param inputImage The source image.
* \param outputImage Binarized image.
* \param maxEdgeWidth The maximum gradient length to consider.
* \param minEdgeMagnitude The minimum color difference in a gradient.
*/
CV_EXPORTS void binarizeMokji(const cv::Mat& inputImage, cv::Mat& outputImage,
                              size_t maxEdgeWidth = 3, size_t minEdgeMagnitude = 20);

}

#endif //PRLIB_BINARIZEMOKJI_H
