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

#include "binarizeFeng.h"

#include <stdexcept>

#include <opencv2/imgproc/imgproc.hpp>

void prl::binarizeFeng(
        cv::Mat& inputImage, cv::Mat& outputImage,
        int windowSize,
        double thresholdCoefficient_alpha1,
        double thresholdCoefficient_k1,
        double thresholdCoefficient_k2,
        double thresholdCoefficient_gamma,
        int morphIterationCount)
{
    if (inputImage.empty())
    {
        throw std::invalid_argument("Input image for binarization is empty");
    }

    if (!((windowSize > 1) && ((windowSize % 2) == 1)))
    {
        throw std::invalid_argument("Window size must satisfy the following condition: \
			( (windowSize > 1) && ((windowSize % 2) == 1) ) ");
    }

    if (inputImage.channels() != 1)
    {
        cv::cvtColor(inputImage, inputImage, CV_BGR2GRAY);
    }

    const int usedFloatType = CV_64FC1;

    //! parameters and constants of algorithm
    int w = std::min(windowSize, std::min(inputImage.cols, inputImage.rows));;
    int wSqr = w * w;
    double wSqrBack = 1.0 / static_cast<double>(wSqr);

    cv::Mat localMeanValues;
    cv::Mat localDevianceValues;

    cv::Rect processingRect(w / 2, w / 2, inputImage.cols - w, inputImage.rows - w);

    {
        //! add borders
        cv::copyMakeBorder(inputImage, inputImage, w / 2, w / 2, w / 2, w / 2, cv::BORDER_REPLICATE);

        cv::Mat integralImage;
        cv::Mat integralImageSqr;

        //! get integral image, ...
        cv::integral(inputImage, integralImage, integralImageSqr, usedFloatType);
        //! ... crop it and ...
        integralImage = integralImage(cv::Rect(1, 1, integralImage.cols - 1, integralImage.rows - 1));
        //! get square
        integralImageSqr =
                integralImageSqr(cv::Rect(1, 1, integralImageSqr.cols - 1, integralImageSqr.rows - 1));

        //! create storage for local means
        localMeanValues = cv::Mat(integralImage.size() - processingRect.size(), usedFloatType);

        //! create filter for local means calculation
        cv::Mat localMeanFilterKernel = cv::Mat::zeros(w, w, usedFloatType);
        localMeanFilterKernel.at<double>(0, 0) = wSqrBack;
        localMeanFilterKernel.at<double>(w - 1, 0) = -wSqrBack;
        localMeanFilterKernel.at<double>(w - 1, w - 1) = wSqrBack;
        localMeanFilterKernel.at<double>(0, w - 1) = -wSqrBack;
        //! get local means
        cv::filter2D(integralImage(processingRect), localMeanValues, usedFloatType,
                     localMeanFilterKernel, cv::Point(-1, -1), 0.0, cv::BORDER_REFLECT);

        //! create storage for local deviations
        cv::Mat localMeanValuesSqr = localMeanValues.mul(localMeanValues); // -V678

        //! create filter for local deviations calculation
        cv::Mat localWeightedSumsFilterKernel = localMeanFilterKernel;

        //! get local deviations
        cv::filter2D(integralImageSqr(processingRect), localDevianceValues, usedFloatType,
                     localWeightedSumsFilterKernel);

        localDevianceValues -= localMeanValuesSqr;
        cv::sqrt(localDevianceValues, localDevianceValues);
    }

    //! calculate Feng thresholds
    double imageMin;
    cv::minMaxLoc(inputImage, &imageMin);
    double alpha1 = thresholdCoefficient_alpha1; // (0.1 + 0.2) / 2.0;
    double k1 = thresholdCoefficient_k1; // (0.15 + 0.25) / 2.0;
    double k2 = thresholdCoefficient_k2; // (0.01 + 0.05) / 2.0;
    double gamma = thresholdCoefficient_gamma; // 2.0;

    cv::Mat Rs = localDevianceValues;

    cv::Mat thresholdsValues;

    {
        cv::Mat tmpAlpha1;
        cv::divide(localDevianceValues, Rs, tmpAlpha1);
        cv::Mat tmpAlpha2;
        cv::pow(tmpAlpha1, gamma, tmpAlpha2);

        cv::Mat alpha2 = k1 * tmpAlpha2;
        cv::Mat alpha3 = k2 * tmpAlpha2;

        //Mat thresholdsValues = (1 - alpha1) * localMeanValues + alpha2.mul(tmpAlpha1).mul(localMeanValues - imageMin) + alpha3 * imageMin;

        double c1 = 1.0 - alpha1;
        cv::Mat c2 = tmpAlpha2.mul(tmpAlpha1);
        tmpAlpha2.release();

        addWeighted(alpha3, imageMin, c2, -imageMin, 0.0, tmpAlpha1);
        cv::Mat c3 = tmpAlpha1;
        //Mat thresholdsValues = localMeanValues.mul(c2 + c1) + c3;
        thresholdsValues = c2 + c1;
        thresholdsValues = thresholdsValues.mul(localMeanValues);
        thresholdsValues += c3;
    }

    thresholdsValues.convertTo(thresholdsValues, CV_8UC1);

    //! get binarized image
    outputImage = inputImage(processingRect) > thresholdsValues;

    //! apply morphology operation if them required
    if (morphIterationCount > 0)
    {
        cv::dilate(outputImage, outputImage, cv::Mat(), cv::Point(-1, -1), morphIterationCount);
        cv::erode(outputImage, outputImage, cv::Mat(), cv::Point(-1, -1), morphIterationCount);
    }
    else
    {
        if (morphIterationCount < 0)
        {
            cv::erode(outputImage, outputImage, cv::Mat(), cv::Point(-1, -1), -morphIterationCount);
            cv::dilate(outputImage, outputImage, cv::Mat(), cv::Point(-1, -1), -morphIterationCount);
        }
    }
}
