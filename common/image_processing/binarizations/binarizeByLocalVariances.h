#ifndef PRLIB_BINARIZEBYLOCALVARIANCES_H
#define PRLIB_BINARIZEBYLOCALVARIANCES_H

#include <opencv2/core/core.hpp>

namespace prl
{
    CV_EXPORTS void binarizeByLocalVariances(cv::Mat& inputImage, cv::Mat& outputImage, double varianceThresholdCoeff = 0.125,
                                             int minResultVariance = 25, double gamma = 2.0);

    CV_EXPORTS void binarizeByLocalVariancesWithoutFilters(cv::Mat& inputImage, cv::Mat& outputImage, double varianceThresholdCoeff = 0.125,
                                                           int minResultVariance = 10);
}

#endif //PRLIB_BINARIZEBYLOCALVARIANCES_H
