#ifndef PRLIB_wienerFilter_h
#define PRLIB_wienerFilter_h

#include <opencv2/core/core.hpp>

namespace prl
{

void wienerFilter(
		cv::Mat& inputImage,
		cv::Mat& outputImage,
		int filterKernelWidth = 11, int filterKernelHeight = 11,
		double coeffWiener = 1,
		double sigmaGauss = 5);

}
#endif // PRLIB_wienerFilter_h
