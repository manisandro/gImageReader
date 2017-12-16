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

#include "imageLibCommon.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <stdexcept>
#include <vector>

#include <opencv2/core/core.hpp>
#include <opencv2/imgproc/imgproc.hpp>

void ExtractLayer(const cv::Mat& inputImage,
                  const cv::Scalar& lowerBoundary, const cv::Scalar& upperBoundary,
                  cv::Mat& outputImage, const cv::Scalar& defaultColor)
{
    if (inputImage.empty())
    {
        throw std::invalid_argument("Image for layer extraction is empty");
    }

    cv::Mat mask;

    //! create mask corresponding of color boundaries
    cv::inRange(inputImage, lowerBoundary, upperBoundary, mask);

    //! extract layer by mask
    ExtractLayer(inputImage, mask, outputImage, defaultColor);
}

void ExtractLayer(const cv::Mat& inputImage, cv::Mat& mask, cv::Mat& outputImage, const cv::Scalar& defaultColor)
{
    if (inputImage.empty())
    {
        throw std::invalid_argument("Image for layer extraction is empty");
    }

    if (mask.empty())
    {
        throw std::invalid_argument("Mask for layer extraction is empty");
    }

    if (inputImage.size() != mask.size())
    {
        throw std::invalid_argument("Sizes of image for layer extraction and mask are different");
    }

    if (!((mask.channels() == 1) || (inputImage.channels() == mask.channels())))
    {
        throw std::invalid_argument("A number of channels in mask is greater than one but isn\'t equal to \
			a number of channels of processed image");
    }

    std::vector<cv::Mat> maskChannels(mask.channels());
    split(mask, maskChannels);

    {
        // scale mask to [0;1]
        double minValue, maxValue;
        cv::minMaxLoc(maskChannels[0], &minValue, &maxValue);
        if (maxValue > 1)
        {
            mask *= 1.0f / maxValue;
        }
    }

    if (mask.type() != CV_8UC1)
    {
        mask.convertTo(mask, CV_8UC1);
    }

    cv::split(mask, maskChannels);

    std::vector<cv::Mat> sourceImageChannels(inputImage.channels());
    std::vector<cv::Mat> destImageChannels(sourceImageChannels.size());
    cv::split(inputImage, sourceImageChannels);

    for (int channelNo = 0; channelNo < inputImage.channels(); ++channelNo)
    {
        //! if we have single channel mask then use it for each channel
        int maskChannelNo = mask.channels() > 1 ? channelNo : 0;

        //! inverse mask for background
        cv::Mat backgroundMask = 1 - maskChannels[maskChannelNo];

        //! copy masked data to new image
        destImageChannels[channelNo] = cv::Mat(inputImage.size(), CV_8UC1);
        destImageChannels[channelNo].setTo(0);
        destImageChannels[channelNo].setTo(defaultColor[channelNo], backgroundMask);
        destImageChannels[channelNo] += sourceImageChannels[channelNo].mul(
                maskChannels[maskChannelNo]);
    }

    cv::merge(destImageChannels, outputImage);
}

void EqualizeLayerHists(const cv::Mat& inputImage, cv::Mat& outputImage)
{
    if (inputImage.empty())
    {
        throw std::invalid_argument("Image for histograms equalization is empty");
    }

    std::vector<cv::Mat> layers(inputImage.channels());
    cv::split(inputImage, layers);

    //! histogram equalization for each channel
    for (size_t i = 0; i < layers.size(); ++i)
    {
        cv::equalizeHist(layers[i], layers[i]);
    }

    cv::merge(layers, outputImage);
}

//! This function searches local minimal and maximal values in histogram
void GetHistExtremums(cv::Mat& hist,
                      std::vector<int>& histLocalMinPositions,
                      std::vector<int>& histLocalMaxPositions,
                      float delta)
{
    if (hist.empty())
    {
        throw std::invalid_argument("Histogram for extremums extration is empty");
    }

    histLocalMinPositions.clear();
    histLocalMaxPositions.clear();

    std::vector<float> histVector;

    bool isHistInColumns = (hist.cols > hist.rows);

    //! copy source histogram to vector
    if (isHistInColumns)
    {
        for (int i = 0; i < hist.cols; ++i)
        {
            histVector.push_back(hist.at<float>(0, i));
        }
    }
    else
    {
        for (int i = 0; i < hist.rows; ++i)
        {
            histVector.push_back(hist.at<float>(i, 0));
        }
    }

    float minValue = std::numeric_limits<float>::infinity();
    float maxValue = -std::numeric_limits<float>::infinity();
    //int minValuePos = 0;
    //int maxValuePos = 0;

    bool isMaxPosLooked = false;

    for (size_t i = 0; i < histVector.size(); ++i)
    {
        if (histVector[i] < minValue)
        {
            minValue = histVector[i];
            //minValuePos = static_cast<int>(i);
        }
        if (histVector[i] > maxValue)
        {
            maxValue = histVector[i];
            //maxValuePos = static_cast<int>(i);
        }

        if (isMaxPosLooked)
        {
            if (histVector[i] < maxValue - delta)
            {
                histLocalMaxPositions.push_back(static_cast<int>(i));
                minValue = histVector[i];
                //minValuePos = static_cast<int>(i);
                isMaxPosLooked = !isMaxPosLooked;
            }
        }
        else
        {
            if (histVector[i] > minValue + delta)
            {
                histLocalMinPositions.push_back(static_cast<int>(i));
                maxValue = histVector[i];
                //maxValuePos = static_cast<int>(i);
                isMaxPosLooked = !isMaxPosLooked;
            }
        }
    }
}

//! Get histograms for each channel of color image
void GetColorLayersHists(cv::Mat& inputImage, std::vector<cv::Mat>& histsArray, int histSize)
{
    if (inputImage.empty())
    {
        throw std::invalid_argument("Image for histograms extraction is empty");
    }

    histsArray.resize(inputImage.channels());

    const float rangeMin = 0.0f;
    const float rangeMax = 256.0f;
    bool uniform = true;
    bool accumulate = false;
    float range[] = {rangeMin, rangeMax};
    const float* histRange = {range};

    std::vector<cv::Mat> channels;
    cv::split(inputImage, channels);

    for (int channelNo = 0; channelNo < inputImage.channels(); ++channelNo)
    {
        cv::calcHist(&channels[channelNo], 1, 0, cv::Mat(), histsArray[channelNo],
                     1, &histSize, &histRange, uniform, accumulate);
    }

}

void CannyEdgeDetection(cv::Mat& imageToProc, cv::Mat& resultCanny, int GaussianBlurKernelSize,
                        double CannyUpperThresholdCoeff, double CannyLowerThresholdCoeff,
                        int CannyMorphIters)
{
    if (imageToProc.empty())
    {
        throw std::invalid_argument("Image for histograms extraction is empty");
    }

    if (GaussianBlurKernelSize < 3)
    {
        throw std::invalid_argument("Gaussian blur kernel size is lesser than 3");
    }

    if (CannyUpperThresholdCoeff < 0 || CannyUpperThresholdCoeff > 1)
    {
        throw std::invalid_argument("Canny upper threshold coefficient isn\'t in range [0;1]");
    }

    if (CannyLowerThresholdCoeff < 0 || CannyLowerThresholdCoeff > 1)
    {
        throw std::invalid_argument("Canny lower threshold coefficient isn\'t in range [0;1]");
    }

    if (CannyLowerThresholdCoeff > CannyUpperThresholdCoeff)
    {
        throw std::invalid_argument("Canny lower threshold coefficient is greater than \
			Canny upper threshold coefficient");
    }

    cv::Mat space = imageToProc.clone();

    //! Split channels
    std::vector<cv::Mat> colorPlanes(space.channels());
    cv::split(space, colorPlanes);

    resultCanny = cv::Mat::zeros(space.size(), CV_8UC1);
    // for each channel:
    for (size_t i = 0; i < colorPlanes.size(); ++i)
    {
        cv::Mat channel;

        //! 1. make Gaussian blur if it required
        if (GaussianBlurKernelSize > 0)
        {
            int kernelSize = GaussianBlurKernelSize;
            cv::GaussianBlur(colorPlanes[i], channel, cv::Size(kernelSize, kernelSize), 0);
        }

        //! 2. get Otsu threshold...
        cv::Mat tmp;
        double otsuThresholdValue = cv::threshold(channel, tmp, 0, 255,
                                                  CV_THRESH_BINARY | CV_THRESH_OTSU);

        //! ... and calculate thresholds for ...
        double upperCoeff = CannyUpperThresholdCoeff;
        double lowerCoeff = CannyLowerThresholdCoeff;

        double upper = upperCoeff * otsuThresholdValue;
        double lower = lowerCoeff * upper;

        //! ... 3. Canny edge detection
        Canny(channel, channel, lower, upper);

        //! 4. approve morphology operations if them required
        if (CannyMorphIters > 0)
        {
            cv::dilate(channel, channel, cv::Mat(), cv::Point(-1, -1), CannyMorphIters);
            cv::erode(channel, channel, cv::Mat(), cv::Point(-1, -1), CannyMorphIters);
        }

        if (CannyMorphIters < 0)
        {
            cv::erode(channel, channel, cv::Mat(), cv::Point(-1, -1), -CannyMorphIters);
            cv::dilate(channel, channel, cv::Mat(), cv::Point(-1, -1), -CannyMorphIters);
        }

        //! 5. combine result by logical OR
        resultCanny = resultCanny | channel;
    }
}

//! Adaptive histogram enhancing for single channel image.
void EnhanceLocalContrastByCLAHE_1(cv::Mat& src, cv::Mat& dst, double CLAHEClipLimit,
                                   bool equalizeHistFlag)
{
    if (src.empty())
    {
        throw std::invalid_argument("Image for histograms enhancing is empty");
    }

    cv::Mat procImage = src.clone();

    cv::Ptr<cv::CLAHE> clahe = cv::createCLAHE();
    clahe->setClipLimit(CLAHEClipLimit);

    clahe->apply(procImage, dst);

    if (equalizeHistFlag)
    {
        cv::equalizeHist(dst, dst);
    }
}

//! Adaptive histogram enhancing for multiple channel image.
void EnhanceLocalContrastByCLAHE_MCh(cv::Mat& src, cv::Mat& dst, const double CLAHEClipLimit,
                                     const bool equalizeHistFlag)
{
    if (src.empty())
    {
        throw std::invalid_argument("Image for histograms enhancing is empty");
    }

    cv::Mat procImage = src.clone();

    cv::Ptr<cv::CLAHE> clahe = cv::createCLAHE();
    clahe->setClipLimit(CLAHEClipLimit);

    std::vector<cv::Mat> claheChannels(procImage.channels());
    cv::split(procImage, claheChannels);

    for (auto& channel : claheChannels)
    {
        clahe->apply(channel, channel);

        if (equalizeHistFlag)
        {
            cv::equalizeHist(channel, channel);
        }
    }

    cv::merge(claheChannels, dst);
}

void EnhanceLocalContrastByCLAHE(cv::Mat& inputImage, cv::Mat& outputImage, double CLAHEClipLimit, bool equalizeHistFlag)
{
    if (inputImage.empty())
    {
        throw std::invalid_argument("Image for histograms enhancing is empty");
    }

    if (inputImage.channels() == 1)
    {
        // single channel image
        EnhanceLocalContrastByCLAHE_1(inputImage, outputImage, CLAHEClipLimit, equalizeHistFlag);
    }
    else
    {
        // multi channel image
        EnhanceLocalContrastByCLAHE_MCh(inputImage, outputImage, CLAHEClipLimit, equalizeHistFlag);
    }
}

void MatToLocalVarianceMap(const cv::Mat& image, cv::Mat& varianceMap, const int kernelSize)
{
    if (image.empty())
    {
        throw std::invalid_argument("Image for local variance map extraction is empty");
    }

    if (!(image.type() == CV_8UC1 || image.type() == CV_8UC3))
    {
        throw std::invalid_argument("Image for local variance map extraction has unsupported type \
			(required 8 or 24 bits per pixel)");
    }

    if (kernelSize <= 1 || (kernelSize % 2) == 0)
    {
        throw std::invalid_argument("Invalid kernel size (required: (kernelSize > 1) && ((kernelSize % 2) != 0) )");
    }

    //! we work with float data
    cv::Mat imageFloat;
    image.convertTo(imageFloat, CV_32FC3, 1, 0);

    //! images used for variance_map calculation
    cv::Mat imageToProc;
    cv::Mat imageToProcSqr;

    //! filter kernel parameters for (kernel_size x kernel_size) area
    const float areaPixelsCount = static_cast<float>(kernelSize * kernelSize);

    //! minimal allowable variance value
    const float varianceMinValue = 0.01f;

    //! create filter kernel for variance calculation
    cv::Mat kernel = cv::Mat::ones(kernelSize, kernelSize, CV_32F);

    //! sum(x[i])
    cv::filter2D(imageFloat, imageToProc, -1, kernel, cv::Point(-1, -1), 0, cv::BORDER_REPLICATE);
    //! sum(x[i])^2
    imageToProc = imageToProc.mul(imageToProc); // -V678

    //! x[i]^2
    imageToProcSqr = imageFloat.clone();
    imageToProcSqr = imageToProcSqr.mul(imageToProcSqr); // -V678

    //! sum(x[i]^2)
    cv::filter2D(imageToProcSqr, imageToProcSqr, -1, kernel,
                 cv::Point(-1, -1), 0, cv::BORDER_REPLICATE);

    //! calculate local variance map
    imageToProcSqr *= areaPixelsCount;
    imageToProcSqr -= imageToProc;
    imageToProcSqr *= (1 / (areaPixelsCount * areaPixelsCount));
    varianceMap = imageToProcSqr;
    //varianceMap = (areaPixelsCount * imageToProcSqr - imageToProc) /
    //	(areaPixelsCount * areaPixelsCount);

    //! If local variance value in point (x, y) above than variance_min_value
    //! (it is constant which empirically defined as 0.01)
    //! then variance in this point is assigned to variance_min_value.
    // if (variance_map(x,y) < variance_min_value) variance_map(x,y) = variance_min_value;
    cv::threshold(
            varianceMap, imageToProc,
            varianceMinValue, varianceMinValue,
            cv::THRESH_BINARY_INV);
    cv::threshold(
            varianceMap, varianceMap,
            varianceMinValue, varianceMinValue,
            cv::THRESH_TOZERO);
    varianceMap += imageToProc;
}

bool IsContourClosed(const std::vector<cv::Point>& contour, int maxDistance)
{
    if (contour.empty())
    {
        throw std::invalid_argument("Contour is empty");
    }

    return (
            std::max(
                    std::abs(contour.front().x - contour.back().x),
                    std::abs(contour.front().y - contour.back().y)
            ) <= maxDistance
    );
}

int ContourChildrenCount(const int contourNo, std::vector<cv::Vec4i>& hierarchy, bool includeSubchildren)
{
    if (hierarchy.empty())
    {
        throw std::invalid_argument("Contours hierarchy is empty");
    }

    //! is contour number valid?
    if (contourNo < 0 || (size_t) contourNo >= hierarchy.size())
    {
        return -1;
    }

    //! Does selected contour have children?
    if (hierarchy[contourNo][2] == -1)
    {
        return 0;
    }

    //! for each child
    int childrenCount = 0;
    int childNo = hierarchy[contourNo][2]; // the first child

    do
    {
        ++childrenCount;

        if (includeSubchildren)
        {
            //! get count of children of child
            childrenCount += ContourChildrenCount(childNo, hierarchy, includeSubchildren);
        }

        //! next child
        childNo = hierarchy[childNo][0];

    }
    while (childNo >= 0);

    return childrenCount;
}

bool IsContourCorrect(std::vector<cv::Point>& contour, cv::Mat& image, size_t contourMinSize,
                      float contourAreaCoeff,
                      float aspectRatioRangeMin, float aspectRatioRangeMax)
{
    if (contour.size() <= contourMinSize)
    {
        // contour too small
        return false;
    }

    //! get bounding rectangle
    cv::Rect contourBoundingRectangle = boundingRect(contour);
    const float coeff = contourAreaCoeff;

    bool isContourTooLong = (contourBoundingRectangle.width > coeff * image.cols);
    bool isContourTooHigh = (contourBoundingRectangle.height > coeff * image.rows);
    if (isContourTooLong || isContourTooHigh)
    {
        return false;
    }

    cv::RotatedRect contourRotatedRectangle = minAreaRect(contour);
    double rectangleAspectRatio = static_cast<double>(contourRotatedRectangle.size.width) /
                                  static_cast<double>(contourRotatedRectangle.size.height);

    bool isRectangleAspectRatioTooSmall = (rectangleAspectRatio < aspectRatioRangeMin);
    bool isRectangleAspectRatioTooBig = (rectangleAspectRatio > aspectRatioRangeMax);
    if (isRectangleAspectRatioTooSmall || isRectangleAspectRatioTooBig)
    {
        return false;
    }

    return true;
}

bool IsContourUncorrect(std::vector<cv::Point>& contour, cv::Mat& image, size_t contourMinSize,
                        float contourAreaCoeff, float aspectRatioRangeMin,
                        float aspectRatioRangeMax)
{
    return !IsContourCorrect(contour, image, contourMinSize, contourAreaCoeff,
                             aspectRatioRangeMin, aspectRatioRangeMax);
}

int ContoursDistance(
        std::vector<cv::Point>& contour1,
        std::vector<cv::Point>& contour2)
{
    if (contour1.size() != contour2.size())
    {
        return std::numeric_limits<int>::max();
    }

    int distance = 0;

    double area1 = cv::contourArea(contour1);
    double area2 = cv::contourArea(contour2);

    distance = static_cast<int>(std::abs(area1 - area2));

    return distance;
}

void RemoveCloseSizeChildren(
        std::vector<std::vector<cv::Point> >& contours,
        std::vector<cv::Vec4i>& hierarchy,
        std::vector<int>& approvingList)
{
    for (size_t i = 0; i < contours.size(); ++i)
    {
        int childNo = hierarchy[i][2];

        if (childNo == -1)
        {
            continue;
        }

        double area1 = cv::contourArea(contours[i], true);
        double area2 = cv::contourArea(contours[childNo], true);

        double areaDiff = std::abs(std::abs(area1) - std::abs(area2));

        bool isContour1ClockWised = area1 > 0;
        bool isContour2ClockWised = area2 > 0;

        bool haveCountoursDiffentClockWising = !(isContour1ClockWised && isContour2ClockWised);
        bool isAreasDiffSmall = (areaDiff <= std::abs(area1 * 0.2));

        if (isAreasDiffSmall && haveCountoursDiffentClockWising)
        {
            bool hasChildSubChildren = (hierarchy[childNo][2] != -1);
            if (hasChildSubChildren)
            {
                int subChildNo = hierarchy[childNo][2];
                hierarchy[subChildNo][3] = hierarchy[childNo][3];
            }

            hierarchy[i][2] = hierarchy[childNo][2];

            approvingList[childNo] = -1;
            hierarchy[childNo][2] = -1;
            hierarchy[childNo][3] = -1;
        }
    }
}

bool IsContourClockwised(std::vector<cv::Point>& contour)
{
    return (cv::contourArea(contour, true) > 0);
}

void CheckHierarhyLevelRecursively(
        int currentContourNumber,
        std::vector<std::vector<cv::Point> >& contours,
        std::vector<int>& approvingList,
        std::vector<cv::Vec4i>& hierarchy);

void RemoveChildrenContours(std::vector<std::vector<cv::Point> >& contours, std::vector<cv::Vec4i>& hierarchy)
{
    if (contours.empty())
    {
        throw std::invalid_argument("Contours array is empty");
    }

    if (hierarchy.empty())
    {
        throw std::invalid_argument("Contours hierarchy is empty");
    }

    // approving list: 0 -- not processed, 1 -- approved, -1 -- ignored
    std::vector<int> approvingList;
    approvingList.resize(static_cast<int>(contours.size()));

    //RemoveCloseSizeChildren(contours, hierarchy, approvingList);

    CheckHierarhyLevelRecursively(
            0,
            contours,
            approvingList,
            hierarchy);

    std::vector<std::vector<cv::Point> > temporaryContoursArray;

    // copy approved contours to new array
    for (size_t i = 0; i < contours.size(); ++i)
    {
        if (approvingList[i] == 1)
        {
            temporaryContoursArray.push_back(contours[i]);
        }
    }

    contours = temporaryContoursArray;
    temporaryContoursArray.clear();
    //hierarchy.clear();
}

int ContourSubChildrenCount(int contourNo, std::vector<cv::Vec4i>& hierarchy)
{
    if (hierarchy.empty())
    {
        throw std::invalid_argument("Contours hierarchy is empty");
    }

    //! is contour number valid?
    if (contourNo < 0 || (size_t) contourNo >= hierarchy.size())
    {
        return -1;
    }

    //! Does selected contour have children?
    if (hierarchy[contourNo][2] == -1)
    {
        return 0;
    }

    //! for each child
    int childrenCount = 1;
    int childNo = hierarchy[contourNo][2];
    while (hierarchy[childNo][2] >= 0)
    {

        childrenCount += ContourChildrenCount(childNo, hierarchy, true);

        //! next child
        childNo = hierarchy[childNo][2];
    }

    return childrenCount;
}

void CheckHierarhyLevelRecursively(
        int currentContourNumber,
        std::vector<std::vector<cv::Point> >& contours,
        std::vector<int>& approvingList,
        std::vector<cv::Vec4i>& hierarchy)
{
    int nextContour = -1;

    do
    {
        nextContour = hierarchy[currentContourNumber][0];

        std::vector<cv::Point>& currentContour = contours[currentContourNumber];

        if (currentContour.size() < 3)
        {
            approvingList[currentContourNumber] = -1;

            currentContourNumber = nextContour;

            continue;
        }

        int childrenCount = ContourChildrenCount(currentContourNumber, hierarchy, true);
        if (childrenCount == 0)
        {
            approvingList[currentContourNumber] = 1;

            currentContourNumber = nextContour;

            continue;
        }

        const int maxChildrenCount = 6;
        bool isChildrenCountLessMax = (childrenCount < maxChildrenCount);

        if (isChildrenCountLessMax)
        {
            approvingList[currentContourNumber] = 1;

            int childNo = hierarchy[currentContourNumber][2];
            approvingList[childNo] = -1;
            while (hierarchy[childNo][0] >= 0)
            {
                approvingList[hierarchy[childNo][0]] = -1;
                childNo = hierarchy[childNo][0];
            }
        }
        else
        {
            approvingList[currentContourNumber] = -1;

            int firstChildNumber = hierarchy[currentContourNumber][2];
            CheckHierarhyLevelRecursively(
                    firstChildNumber,
                    contours,
                    approvingList,
                    hierarchy);
        }

        currentContourNumber = nextContour;

    }
    while (nextContour != -1);

}

void ScaleToRange(
        const cv::Mat& src, cv::Mat& dst,
        double rangeMin /*= 0.0*/, double rangeMax /*= 255.0*/,
        int dstType /*= CV_8UC1*/)
{
    if (src.empty())
    {
        throw std::invalid_argument("Input image is empty");
    }

    if (src.channels() != 1)
    {
        throw std::invalid_argument("Input image should be single-channel");
    }

    if (fabs(rangeMin - rangeMax) < 10e-8)
    {
        throw std::invalid_argument("Width of range is 0");
    }

    if (rangeMin > rangeMax)
    {
        std::swap(rangeMin, rangeMax);
    }

    double minValue, maxValue;
    cv::minMaxLoc(src, &minValue, &maxValue);

    if (src.channels() != 1)
    {
        throw std::invalid_argument("Input image min == max");
    }

    double alpha = (rangeMax - rangeMin) / (maxValue - minValue);
    double beta = -minValue * alpha + rangeMin;

    src.convertTo(dst, dstType, alpha, beta);
}

bool IsQuadrangularConvex(const std::vector<cv::Point2f>& resultContour)
{
    if (resultContour.empty())
    {
        return false;
    }

    //! Find convex hull of points
    std::vector<cv::Point2f> convexHullPoints;
    cv::convexHull(cv::Mat(resultContour), convexHullPoints, false);

    return (convexHullPoints.size() == 4);
}

bool cropVerticesOrdering(std::vector<cv::Point>& pt)
{
    if (pt.empty())
    {
        throw std::invalid_argument("Contour for ordering is empty");
    }

    //! This should be 4 for rectangles, but it allows to be extended to more complex cases
    int verticesNumber = static_cast<int>(pt.size());
    if (verticesNumber != 4)
    {
        throw std::invalid_argument("Points number in input contour isn't equal 4");
    }

    //! Find convex hull of points
    std::vector<cv::Point> points;
    points = pt;

    // indices of convex vertices
    std::vector<int> indices;
    cv::convexHull(points, indices, false);

    //! Find top left point, as starting point. This is the nearest to (0.0) point
    int minDistanceIdx = 0;
    double minDistanceSqr =
            pt[indices[0]].x * pt[indices[0]].x +
            pt[indices[0]].y * pt[indices[0]].y;

    for (int i = 1; i < verticesNumber; ++i)
    {
        double distanceSqr =
                pt[indices[i]].x * pt[indices[i]].x +
                pt[indices[i]].y * pt[indices[i]].y;

        if (distanceSqr < minDistanceSqr)
        {
            minDistanceIdx = i;
            minDistanceSqr = distanceSqr;
        }
    }

    //! Now starting point index in source array is in indices[minDistanceIdx]
    //! convex hull vertices are sorted clockwise in indices array, so transfer them to result
    //! beginning from starting point to the end of array...
    std::vector<cv::Point> ret(verticesNumber);
    int idx = 0;
    for (int i = minDistanceIdx; i < verticesNumber; ++i, ++idx)
    {
        ret[idx] = cv::Point(pt[indices[i]].x, pt[indices[i]].y);
    }

    //! ... and from the begin of array up to starting point
    for (int i = 0; i < minDistanceIdx; ++i, ++idx)
    {
        ret[idx] = cv::Point(pt[indices[i]].x, pt[indices[i]].y);
    }

    pt = ret;

    return true;
}


cv::Mat getGaussianKernel2D(const int ksize, double sigma)
{
    double p_max = 0.0, p_temp = 0.0;

    sigma *= 2.0;

    cv::Mat kernel(ksize, ksize, CV_64FC1);
    kernel.setTo(0);

    for (int i = 0; i < ksize; i++)
    {
        for (int j = 0; j < ksize; j++)
        {
            p_temp = (1.0 / (2 * CV_PI * sigma)) *
                     std::exp((-1) * (std::pow(i - ksize / 2, 2.0) +
                                      std::pow(j - ksize / 2, 2.0)) / (std::pow(2 * sigma, 2)));

            kernel.at<double>(i, j) = p_temp;

            if (p_temp > p_max)
            {
                p_max = p_temp;
            }
        }
    }

    kernel /= p_max;

    return kernel;
}

std::pair<cv::Point2f, cv::Point2f> fromVec2f(cv::Vec2f vec)
{
    float rho = vec[0], theta = vec[1];
    cv::Point2f pt1, pt2;
    double a = std::cos(theta), b = std::sin(theta);
    double x0 = a * rho, y0 = b * rho;
    pt1.x = static_cast<float>(cvRound(x0 + 1000 * (-b)));
    pt1.y = static_cast<float>(cvRound(y0 + 1000 * (a)));
    pt2.x = static_cast<float>(cvRound(x0 - 1000 * (-b)));
    pt2.y = static_cast<float>(cvRound(y0 - 1000 * (a)));

    return std::make_pair(pt1, pt2);
}