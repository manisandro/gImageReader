#ifndef PRLIB_HoughLine_h__
#define PRLIB_HoughLine_h__

#include <vector>

#include <opencv2/core/core.hpp>

namespace prl
{

bool findHoughLineContour(cv::Mat& inputImage,
                          std::vector<cv::Point>& resultContour);

}
#endif // PRLIB_HoughLine_h__
