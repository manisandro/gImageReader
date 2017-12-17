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

#include "wienerFilter.h"

#include <cmath>
#include <complex>
#include <memory>
#include <stdexcept>
#include <vector>

#include <opencv2/highgui/highgui.hpp>

void prl::wienerFilter(cv::Mat& inputImage, cv::Mat& outputImage,
                       int filterKernelWidth, int filterKernelHeight,
                       double coeffWiener, double sigmaGauss)
{
    if (inputImage.empty())
    {
        throw std::invalid_argument("Input image for deblurring is empty");
    }

    if (!((filterKernelWidth > 1) && (filterKernelHeight > 1)))
    {
        throw std::invalid_argument("Parameters must satisfy the following condition: \
			( (filterKernelWidth > 1) && (filterKernelHeight > 1) ) ");
    }

    if (sigmaGauss <= 0)
    {
        throw std::invalid_argument("Parameters must satisfy the following condition: \
			( sigmaGauss > 0 ) ");
    }

    int inputImageHeight = inputImage.rows;
    int inputImageWidth = inputImage.cols;


    //////////////////////////////////////////////////////////////////////////

    cv::Mat kernelImage = cv::Mat(filterKernelHeight, filterKernelWidth, CV_64FC1);

    double sigmaSqr = sigmaGauss * sigmaGauss;

    double amplitude = 1 / (sigmaGauss * std::sqrt(2 * CV_PI));

    int halfFilterKernelWidth = filterKernelWidth / 2;
    int halfFilterKernelHeight = filterKernelHeight / 2;

    for (int i = 0; i < filterKernelHeight; ++i)
    {
        int y = i - halfFilterKernelHeight;
        const double ySqr = y * y;

        for (int j = 0; j < filterKernelWidth; ++j)
        {
            int x = j - halfFilterKernelWidth;
            double xSqr = x * x;

            double exponent = static_cast<double>((ySqr + xSqr) / (2.0 * sigmaSqr));

            kernelImage.at<double>(i, j) = amplitude * exp(-exponent);
        }
    }

    //////////////////////////////////////////////////////////////////////////

    cv::Mat kernelMatrix = cv::Mat::zeros(inputImageHeight, inputImageWidth, CV_64FC1);

    {
        double sumKernelElements = 0.0;

        int inputImageWidthHalf = inputImageWidth / 2;
        int inputImageHeightHalf = inputImageHeight / 2;

        /*std::shared_ptr<double[]> kernelTempMatrix(
            new double[inputImageWidth * inputImageWidth], std::default_delete<double[]>());*/

        std::vector<double> kernelTempMatrix(inputImageWidth * inputImageWidth);

        for (int y = 0; y < inputImageHeight; ++y)
        {
            for (int x = 0; x < inputImageWidth; ++x)
            {
                int index = y * inputImageWidth + x;
                double value = 0.0;

                // if we are in the kernel area (of small kernelImage), then take pixel values.
                // Otherwise keep 0
                if (std::abs(x - inputImageWidthHalf) < (filterKernelWidth - 2) / 2 &&
                    std::abs(y - inputImageHeightHalf) < (filterKernelHeight - 2) / 2)
                {
                    int xLocal = x - (inputImageWidth - filterKernelWidth) / 2;
                    int yLocal = y - (inputImageHeight - filterKernelHeight) / 2;

                    value = kernelImage.at<double>(yLocal, xLocal);
                }

                kernelTempMatrix[index] = value;
                sumKernelElements += std::abs(value);
            }
        }

        // Zero-protection
        if (sumKernelElements == 0.0)
        {
            sumKernelElements = 1.0;
        }

        // Normalize
        double sumKernelElementsBack = 1.0 / sumKernelElements;
        for (int i = 0; i < inputImageWidth * inputImageHeight; ++i)
        {
            kernelTempMatrix[i] *= sumKernelElementsBack;
        }

        // Translate kernel, because we don't use centered FFT
        // (by multiply input image on pow(-1, x + y))
        // so we need to translate kernel by (width / 2) to the left
        // and by (height / 2) to the up
        for (size_t y = 0; y < inputImageHeight; ++y)
        {
            for (size_t x = 0; x < inputImageWidth; ++x)
            {
                int xTranslated = (x + inputImageWidth / 2) % inputImageWidth;
                int yTranslated = (y + inputImageHeight / 2) % inputImageHeight;
                (reinterpret_cast<double*>(kernelMatrix.data))[y * inputImageWidth + x] =
                        kernelTempMatrix[yTranslated * inputImageWidth + xTranslated];

// 				kernelMatrix.at<double>(y, x) = 
// 					kernelTempMatrix.get()[yTranslated * inputImageWidth + xTranslated];
            }
        }

    }

    cv::Mat kernelFFT;
    cv::dft(kernelMatrix, kernelFFT, cv::DFT_COMPLEX_OUTPUT);

    //! Rewrite this code using OpenCV functions
    for (int y = 0; y < inputImageHeight; ++y)
    {
        for (int x = 0; x < inputImageWidth; ++x)
        {
            double pointValueReal = kernelFFT.at<std::complex<double> >(y, x).real();
            double pointValueImag = kernelFFT.at<std::complex<double> >(y, x).imag();

            double pointValueRealSqr = pointValueReal * pointValueReal;
            double pointValueImagSqr = pointValueImag * pointValueImag;

            double energyValue = pointValueRealSqr + pointValueImagSqr;

            double wienerValue = pointValueReal / (energyValue + coeffWiener);

            kernelFFT.at<std::complex<double> >(y, x) *= wienerValue;
        }
    }

    //////////////////////////////////////////////////////////////////////////


    std::vector<cv::Mat> inputImageChannels, resultImageChannels;
    cv::split(inputImage, inputImageChannels);

    for (cv::Mat& inputChannel : inputImageChannels)
    {
        cv::Mat realTypeInputChannel;
        inputChannel.convertTo(realTypeInputChannel, CV_64F);

        cv::Mat inputChannelFFT = cv::Mat::zeros(realTypeInputChannel.size(), realTypeInputChannel.type());
        cv::dft(realTypeInputChannel, inputChannelFFT, cv::DFT_COMPLEX_OUTPUT);

        //inputChannelFFT *= 1.0 / (inputImageWidth * inputImageHeight);

        cv::Mat outputChannelFFT;
        cv::mulSpectrums(inputChannelFFT, kernelFFT, outputChannelFFT, 0);

        cv::Mat complexTypeResultChannel;
        cv::idft(outputChannelFFT, complexTypeResultChannel, cv::DFT_SCALE);

        std::vector<cv::Mat> resultRealTypeSubchannels;
        cv::split(complexTypeResultChannel, resultRealTypeSubchannels);

        resultRealTypeSubchannels[0].convertTo(resultRealTypeSubchannels[0], CV_8UC1);
        resultImageChannels.push_back(resultRealTypeSubchannels[0]);
    }

    cv::merge(resultImageChannels, outputImage);
}