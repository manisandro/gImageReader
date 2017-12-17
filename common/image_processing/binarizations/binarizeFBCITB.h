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

#ifndef PRLIB_FBCITB_Binarizator_h
#define PRLIB_FBCITB_Binarizator_h

#include <map>

#include <opencv2/core/core.hpp>

namespace prl
{

/*!
 * \brief List of operations to use in binarization procedure
 * \sa FBCITB_ParamsCodes, FBCITB_ParamsMap
 */
enum class OPERATIONS
{
    // for contours detection
    USE_CANNY = 1,    //!< Use Canny edge detector.
    USE_VARIANCES = 2,    //!< Use local variance values map.

    // Additional operation
    USE_CANNY_ON_VARIANCES = 32,   //!< Apply Canny edge detector to local variance values map.
    USE_OTHER_COLOR_SPACE = 64,   //!< Change processed image color space.
    USE_CLAHE = 128,  //!< Use contrast enhancement.
    USE_BILATERAL = 256,  //!< Use bilateral filtration.
    USE_MORPHOLOGY = 512   //!< Apply morphology operations.
};

/*!
 * \brief List of possible processing parameter codes.
 * \sa OPERATIONS, FBCITB_ParamsMap
 */
enum class FBCITB_ParamsCodes
{
    COLOR_SPACE,                                //!< Color space parameter.
    CLAHE_CLIP_LIMIT,                           //!< CLAHE procedure parameter.
    BILATERAL_FILTER_KERNEL_SIZE,               //!< Bilateral filter kernel size.
    BILATERAL_FILTER_KERNEL_INTENSITY_SIGMA,    //!< Bilateral filter intensity sigma.
    BILATERAL_FILTER_KERNEL_SPATIAL_SIGMA,      //!< Bilateral filter spatial sigma.
    CANNY_GAUSSIAN_BLUR_KERNEL_SIZE,    //!< Gaussian blur kernel size for Canny edge detector.
    CANNY_LOWER_THRESHOLD_COEFF,    //!< Coefficient for lower threshold of Canny edge detector.
    CANNY_UPPER_THRESHOLD_COEFF,    //!< Coefficient for upper threshold of Canny edge detector.
    VARIANCE_MAP_THRESHOLD,         //!< Threshold value for local variance map.
    BOUNDING_RECT_MAX_AREA_COEFF    //!< Coefficient for maximal possible bounding rectangle area.
};

/*!
 * \typedef FBCITB_ParamsMap
 * \brief Map of binarization parameters.
 * \sa FBCITB_ParamsCodes, OPERATIONS
 */
typedef std::map<int, double> FBCITB_ParamsMap;


/*!
 * \brief Implementation of font and background color independent text binarization.
 * \param[in] inputImage Input image.
 * \param[out] outputImage Output image.
 * \param[in] operations Used operations.
 * \param[in] paramsMap Parameters map for operations.
 * \details This is an implementation of "Font and Background Color Independent Text Binarization".
 */
CV_EXPORTS void binarizeFBCITB(
        cv::Mat& inputImage, cv::Mat& outputImage,
        bool useCanny = true, bool useVariancesMap = true,
        bool useCLAHE = true, bool useBilateral = true,
        bool useOtherColorspace = false,
        bool useMorphology = true,
        bool useCannyOnVariances = true,
        double varianceMapThreshold = 200.0,
        double CLAHEClipLimit = 2.0,
        double gaussianBlurKernelSize = 9.0,
        double cannyUpperThresholdCoeff = 0.6,
        double cannyLowerThresholdCoeff = 0.4,
        double boundingRectangleMaxArea = 0.3,
        int bilateralKernelSize = 5,
        double bilateralKernelIntensitySigma = 150.0,
        double bilateralKernelSpatialSigma = 150.0,
        double boundingRectMaxAreaCoeff = 0.3);

}
#endif // PRLIB_FBCITB_Binarizator_h
