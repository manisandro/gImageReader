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

#ifndef PRLIB_ImageLibCommon_h
#define PRLIB_ImageLibCommon_h

#include <vector>

#include <opencv2/core/core.hpp>
#include <opencv2/highgui/highgui.hpp>

/*!
 * \brief Get local variance map for OpenCV Mat.
 * \param[in] image Input image.
 * \param[out] varianceMap Resulting local variance map.
 * \param[in] kernelSize Size of area for variance value estimation.
 * \details This function estimate local variance value in area selected by sliding
 * window with size (kernelSize x kernelSize).
 * Estimated values are stored to corresponding position of variance_map.
 */
void MatToLocalVarianceMap(const cv::Mat& image, cv::Mat& varianceMap,
                           const int kernelSize = 3);

/*!
 * \brief Scale image to defined range
 * \param[in] src Image for range scaling.
 * \param[out] dst Image for result storing.
 * \param[in] rangeMin Lower boundary of range.
 * \param[in] rangeMax Upper boundary of range.
 * \param[in] dstType OpenCV image type of result image.
 */
void ScaleToRange(const cv::Mat& src, cv::Mat& dst,
                  double rangeMin = 0.0, double rangeMax = 255.0,
                  int dstType = CV_8UC1);

/*!
 * \brief Get count of children contours for input contour.
 * \param[in] contourNumber Number of contour in hierarchy.
 * \param[in] hierarchy Description of relations between contours.
 * \param[in] includeSubchildren \parblock
 * If this flag equal true then function calculate children of each child contour.
 * \endparblock
 * \return Count of children contours.
 */
int ContourChildrenCount(const int contourNumber, std::vector<cv::Vec4i>& hierarchy,
                         bool includeSubchildren = false);

/*!
 * \brief Get count of children contours for this contour children.
 * \param[in] contourNumber Number of contour in hierarchy.
 * \param[in] hierarchy Description of relations between contours.
 * \return Count of subchildren contours.
 */
int ContourSubChildrenCount(int contourNumber, std::vector<cv::Vec4i>& hierarchy);

/*!
 * \brief Check on closed contour.
 * \param[in] contour Tested contour.
 * \param[in] maxDistance Maximal distance between contour start and end.
 * \return true if contour is closed.
 */
bool IsContourClosed(const std::vector<cv::Point>& contour, int maxDistance = 1);

/*!
 * \brief Checks contour for compliance.
 * \param[in] contour Tested contour.
 * \param[in] image Image which contains contour.
 * \param[in] contourMinSize Minimal contour size.
 * \param[in] contourAreaCoeff Coefficient of maximal contour area.
 * \param[in] aspectRatioRangeMin Aspect ratio range minimal value.
 * \param[in] aspectRatioRangeMax Aspect ratio range maximal value.
 * \return true if contour meets requirements
 * \details This function checks contour for compliance:
 * - The aspect ratio is constrained to lie between aspectRatioRangeMin and
 *   aspectRatioRangeMax to remove highly elongated components.
 * - Components larger than contourAreaCoeff times the image dimensions are removed.
 * - Furthermore, small and spurious components with areas less than contourMinSize
 *   pixels are not considered for further processing.
 */
bool IsContourCorrect(std::vector<cv::Point>& contour, cv::Mat& image,
                      size_t contourMinSize = 8,
                      float contourAreaCoeff = 0.7f,
                      float aspectRatioRangeMin = 0.1f,
                      float aspectRatioRangeMax = 10.0f);

/*!
 * \brief Checks contour for compliance.
 * \param[in] contour Tested contour.
 * \param[in] image Image which contains contour.
 * \param[in] contourMinSize Minimal contour size.
 * \param[in] contourAreaCoeff Coefficient of maximal contour area.
 * \param[in] aspectRatioRangeMin Aspect ratio range minimal value.
 * \param[in] aspectRatioRangeMax Aspect ratio range maximal value.
 * \return false if contour meets requirements
 * \details This function checks contour for compliance:
 * - The aspect ratio is constrained to lie between aspectRatioRangeMin and
 *   aspectRatioRangeMax to remove highly elongated components.
 * - Components larger than contourAreaCoeff times the image dimensions are removed.
 * - Furthermore, small and spurious components with areas less than contourMinSize
 *   pixels are not considered for further processing.
 */
bool IsContourUncorrect(std::vector<cv::Point>& contour, cv::Mat& image,
                        size_t contourMinSize = 8,
                        float contourAreaCoeff = 0.7f,
                        float aspectRatioRangeMin = 0.1f,
                        float aspectRatioRangeMax = 10.0f);

/*!
 * \brief Remove children contours.
 * \param[in] contours Array of contours.
 * \param[in] hierarchy Description of relations between contours.
 * \details This function removes all children contours.
 */
void RemoveChildrenContours(std::vector<std::vector<cv::Point>>& contours,
                            std::vector<cv::Vec4i>& hierarchy);

/*!
 * \brief Enhance local contrast.
 * \param[in] inputImage Source image.
 * \param[out] outputImage Destination image.
 * \param[in] CLAHEClipLimit Parameter of local contrast enhancement procedure.
 * \param[in] equalizeHistFlag \parblock
 * Histogram equalization flag (if equal true then histogram should be equalized).
 * \endparblock
 * \details This function enhance local contrast of src using algorithm 
 * <a href="http://en.wikipedia.org/wiki/Adaptive_histogram_equalization">CLAHE</a> and
 * equalize histogram if it is required.
 */
void EnhanceLocalContrastByCLAHE(cv::Mat& inputImage, cv::Mat& outputImage,
                                 double CLAHEClipLimit,
                                 bool equalizeHistFlag);

/*!
 * \brief Edge detection using Canny edge detector.
 * \param[in] imageToProc Input image.
 * \param[out] resultCanny Output image.
 * \param[in] GaussianBlurKernelSize \parblock
 * Kernel size for Gaussian blur (if less or equal than 0 then procedure is not used).
 * \endparblock
 * \param[in] CannyUpperThresholdCoeff Coefficient for upper threshold of Canny edge detector.
 * \param[in] CannyLowerThresholdCoeff Coefficient for lower threshold of Canny edge detector.
 * \param[in] CannyMorphIters \parblock
 * Parameter of erode and dilatation (if equal than 0 then procedures are not used.
 * If above than 0 then dilate is used the first. If less than 0 then erode is used the first).
 * \endparblock
 * \details This function separates image into channels, processes them and combine by logical OR.
 */
void CannyEdgeDetection(cv::Mat& imageToProc, cv::Mat& resultCanny,
                        int GaussianBlurKernelSize = 9,
                        double CannyUpperThresholdCoeff = 1,
                        double CannyLowerThresholdCoeff = 0.5,
                        int CannyMorphIters = 1);

/*!
 * \brief Extract histograms for each image channel.
 * \param[in] input_image Source image.
 * \param[out] hists_array Array to store obtained histograms.
 * \param[in] histSize Size of created histogram.
 */
void GetColorLayersHists(cv::Mat& inputImage, std::vector<cv::Mat>& histsArray,
                         int histSize = 256);

/*!
 * \brief This function finds positions of local minimums and maximums for histogram.
 * \param[in] hist Source histogram.
 * \param[out] histLocalMinPositions Array for local minimums positions.
 * \param[out] histLocalMaxPositions Array for local maximums positions.
 * \param[in] delta Minimal difference of values for local extremum detection.
 */
void GetHistExtremums(cv::Mat& hist,
                      std::vector<int>& histLocalMinPositions,
                      std::vector<int>& histLocalMaxPositions,
                      float delta = 10.0f);

/*!
 * \brief This function applies OpenCV function equalizeHist() for each channel of image.
 * \param[in] inputImage Source image.
 * \param[out] outputImage Result image.
 */
void EqualizeLayerHists(const cv::Mat& inputImage, cv::Mat& outputImage);


/*!
 * \brief Extract layer by mask.
 * \param[in] inputImage Image for layer extraction.
 * \param[in] mask Mask for extracted pixels.
 * \param[out] outputImage Extracted layer.
 * \param[in] defaultColor Default color for masked pixels background.
 */
void ExtractLayer(const cv::Mat& inputImage, cv::Mat& mask, cv::Mat& outputImage,
                  const cv::Scalar& defaultColor = CV_RGB(255, 255, 255));

/*!
 * \brief Extract layer by color range.
 * \param[in] inputImage Image for layer extraction.
 * \param[in] lowerBoundary,upperBoundary Boundaries of color range for extraction.
 * \param[out] outputImage Extracted layer.
 * \param[in] defaultColor Default color for masked pixels background.
 */
void ExtractLayer(const cv::Mat& inputImage,
                  const cv::Scalar& lowerBoundary,
                  const cv::Scalar& upperBoundary,
                  cv::Mat& outputImage,
                  const cv::Scalar& defaultColor = CV_RGB(255, 255, 255));


bool IsQuadrangularConvex(const std::vector<cv::Point2f>& resultContour);

/*!
 * \brief Contour points ordering.
 * \param[inout] pt Contour.
 * \return true if ordering is successful.
 */
bool cropVerticesOrdering(std::vector<cv::Point>& pt);

cv::Mat getGaussianKernel2D(const int ksize, double sigma);

std::pair<cv::Point2f, cv::Point2f> fromVec2f(cv::Vec2f vec);
#endif // PRLIB_ImageLibCommon_h
