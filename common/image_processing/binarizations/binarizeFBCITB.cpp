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

#include "binarizeFBCITB.h"

#include <algorithm>
#include <stdexcept>
#include <vector>

#include <opencv2/highgui/highgui.hpp>
#include <opencv2/imgproc/imgproc.hpp>
#include <opencv2/features2d/features2d.hpp>

#include "imageLibCommon.h"


/*switch (key) {
case CLAHE_CLIP_LIMIT:
        resultValue = 2.0;
break;
case BILATERAL_FILTER_KERNEL_SIZE:
        resultValue = 5;
break;
case BILATERAL_FILTER_KERNEL_INTENSITY_SIGMA:
        resultValue = 150.0;
break;
case BILATERAL_FILTER_KERNEL_SPATIAL_SIGMA:
        resultValue = 150.0;
break;
case CANNY_GAUSSIAN_BLUR_KERNEL_SIZE:
        resultValue = 9.0;
break;
case CANNY_UPPER_THRESHOLD_COEFF:
        resultValue = 0.6;
break;
case CANNY_LOWER_THRESHOLD_COEFF:
        resultValue = 0.4;
break;
case VARIANCE_MAP_THRESHOLD:
        resultValue = 200;
break;
case COLOR_SPACE:
        resultValue = CV_RGB2Luv;
break;
case BOUNDING_RECT_MAX_AREA_COEFF:
        resultValue = 0.3;
break;
default:
return 0.0;
}*/

void prl::binarizeFBCITB(
        cv::Mat& inputImage, cv::Mat& outputImage,
        bool useCanny, bool useVariancesMap,
        bool useCLAHE, bool useBilateral,
        bool useOtherColorspace,
        bool useMorphology,
        bool useCannyOnVariances,
        double varianceMapThreshold,
        double CLAHEClipLimit,
        double gaussianBlurKernelSize,
        double cannyUpperThresholdCoeff,
        double cannyLowerThresholdCoeff,
        double boundingRectangleMaxArea,
        int bilateralKernelSize,
        double bilateralKernelIntensitySigma,
        double bilateralKernelSpatialSigma,
        double boundingRectMaxAreaCoeff)
{
    if (inputImage.empty())
    {
        throw std::invalid_argument("Input image for binarization is empty");
    }

    if (inputImage.type() != CV_8UC3)
    {
        if (inputImage.channels() == 1)
        {
            cv::cvtColor(inputImage, inputImage, CV_GRAY2BGR);
        }
        else
        {
            throw std::invalid_argument("Invalid type of image for binarization (required 24 bits per pixel)");
        }
    }

    //! if Canny detector and local variance operations are not used then use both.
    bool isCannyRequired = useCanny;
    bool isVariancesMapRequired = useVariancesMap;
    if (!isCannyRequired && !isVariancesMapRequired)
    {
        useCanny = useVariancesMap = true;
    }

    cv::Mat imageToProc = inputImage.clone();
    //Mat image_to_show = image_to_proc.clone();

/*    if ((operations & USE_OTHER_COLOR_SPACE) != 0)
    {
        // change color space
        cv::cvtColor(imageToProc, imageToProc,
                     static_cast<int>(GetFBCITBParam(paramsMap, COLOR_SPACE)));
    }*/

    if (useCLAHE)
    {
        //! contrast enhancement
        EnhanceLocalContrastByCLAHE(imageToProc, imageToProc, CLAHEClipLimit, false);
    }

    if (useBilateral)
    {
        //! bilateral filtration
        std::vector<cv::Mat> channelsForBilateralFiltration(imageToProc.channels());
        std::vector<cv::Mat> tempChannelsForBilateralFiltration(imageToProc.channels());

        cv::split(imageToProc, channelsForBilateralFiltration);
        cv::split(imageToProc, tempChannelsForBilateralFiltration);

        //! set parameters of filtration
        int kernelSize = static_cast<int>(bilateralKernelSize);
        double intensitySigma = bilateralKernelIntensitySigma;
        double spatialSigma = bilateralKernelSpatialSigma;

        for (size_t i = 0; i < channelsForBilateralFiltration.size(); ++i)
        {
            cv::bilateralFilter(channelsForBilateralFiltration[i],
                                tempChannelsForBilateralFiltration[i], kernelSize,
                                intensitySigma, spatialSigma);
        }

        cv::merge(tempChannelsForBilateralFiltration, imageToProc);
    }

    cv::Mat resultCanny;

    if (useCanny)
    {
        //! use Canny detector
        int kernelSize = static_cast<int>(gaussianBlurKernelSize);
        double upperCoeff = cannyUpperThresholdCoeff;
        double lowerCoeff = cannyLowerThresholdCoeff;
        CannyEdgeDetection(imageToProc, resultCanny, kernelSize, upperCoeff, lowerCoeff, 1);
    }

    //! get map of local variance
    cv::Mat varianceMap;

    if (useVariancesMap)
    {
        MatToLocalVarianceMap(inputImage, varianceMap);

        varianceMap = (varianceMap > varianceMapThreshold);

        //cv::dilate(variance_map, variance_map, cv::Mat(), cv::Point(-1, -1), 2);
        //cv::erode(variance_map, variance_map, cv::Mat(), cv::Point(-1, -1), 2);

        cv::convertScaleAbs(varianceMap, varianceMap, 255, 0);

        std::vector<cv::Mat> varianceMapChannels(varianceMap.channels());
        cv::split(varianceMap, varianceMapChannels);
        varianceMap = varianceMapChannels[0] & varianceMapChannels[1] & varianceMapChannels[2];

        if (useCannyOnVariances)
        {
            cv::Canny(varianceMap, varianceMap, 64, 128);
        }
    }

    if (isCannyRequired && isVariancesMapRequired)
    {
        varianceMap &= resultCanny;
    }

    std::vector<std::vector<cv::Point> > contours;
    std::vector<cv::Vec4i> hierarchy;

    //! contours detection
    if (useVariancesMap)
    {
        cv::findContours(varianceMap, contours, hierarchy, CV_RETR_TREE,
                         CV_CHAIN_APPROX_SIMPLE, cv::Point(0, 0));
    }

    if (isCannyRequired && !isVariancesMapRequired)
    {
        cv::findContours(resultCanny, contours, hierarchy, CV_RETR_TREE,
                         CV_CHAIN_APPROX_SIMPLE, cv::Point(0, 0));
    }

    if (contours.empty())
    {
        outputImage = cv::Mat(imageToProc.size(), CV_8UC1);
        outputImage.setTo(255);

        return;
    }

    RemoveChildrenContours(contours, hierarchy);

    std::vector<std::vector<cv::Point> > tempContours;

    double m_boundingRectMaxAreaCoeff = boundingRectangleMaxArea;
    int boundingRectMaxArea = static_cast<int>(m_boundingRectMaxAreaCoeff * imageToProc.cols *
                                               imageToProc.rows);

    //! get bounding rectangles
    std::vector<cv::Rect> contourBoundingRectangles;
    for (size_t i = 0; i < contours.size(); ++i)
    {
        cv::Rect contourBoundingRectangle = cv::boundingRect(contours[i]);

        bool isBoundingRectangleAreaBiggerThanMin = contourBoundingRectangle.area() > 10;
        bool isBoundingRectangleAreaLesserThanMax =
                contourBoundingRectangle.area() < boundingRectMaxArea;

        bool isBoundingRectangleWidthBiggerThanMax =
                contourBoundingRectangle.width >= static_cast<int>(0.8 * imageToProc.cols);
        bool isBoundingRectangleHeightBiggerThanMax =
                contourBoundingRectangle.height >= static_cast<int>(0.8 * imageToProc.rows);

        if (isBoundingRectangleAreaBiggerThanMin && isBoundingRectangleAreaLesserThanMax &&
            !isBoundingRectangleWidthBiggerThanMax && !isBoundingRectangleHeightBiggerThanMax)
        {
            tempContours.push_back(contours[i]);
            contourBoundingRectangles.push_back(contourBoundingRectangle);
        }

    }

    contours = tempContours;
    tempContours.clear();

    if (imageToProc.channels() == 3)
    {
        cv::cvtColor(imageToProc, imageToProc, CV_BGR2GRAY);
        cv::cvtColor(imageToProc, imageToProc, CV_GRAY2BGR);
    }

    cv::Mat resultImage(imageToProc.size(), CV_8UC1);
    cv::Mat resultImage2(imageToProc.size(), CV_8UC1);
    resultImage.setTo(255);
    resultImage2.setTo(255);

    for (size_t i = 0; i < contours.size(); ++i)
    {

        int N_E = static_cast<int>(contours[i].size());

        //! foreground color
        cv::Vec3f F_EB;
        for (size_t pointNo = 0; pointNo < contours[i].size(); ++pointNo)
        {
            F_EB += imageToProc.at<cv::Vec3b>(contours[i][pointNo]);
        }
        F_EB /= N_E;

        //! get points near corners of bounding rectangle
        std::vector<cv::Point> B;
        cv::Point point;
        cv::Rect imageRectangle(0, 0, imageToProc.cols, imageToProc.rows);

        point = (cv::Point(contourBoundingRectangles[i].x - 1, contourBoundingRectangles[i].y - 1));
        if (point.inside(imageRectangle))
        { B.push_back(point); }
        point = (cv::Point(contourBoundingRectangles[i].x - 1, contourBoundingRectangles[i].y - 0));
        if (point.inside(imageRectangle))
        { B.push_back(point); }
        point = (cv::Point(contourBoundingRectangles[i].x - 0, contourBoundingRectangles[i].y - 1));
        if (point.inside(imageRectangle))
        { B.push_back(point); }

        point = (cv::Point(contourBoundingRectangles[i].x + contourBoundingRectangles[i].width + 1,
                           contourBoundingRectangles[i].y - 1));
        if (point.inside(imageRectangle))
        { B.push_back(point); }
        point = (cv::Point(contourBoundingRectangles[i].x + contourBoundingRectangles[i].width + 0,
                           contourBoundingRectangles[i].y - 1));
        if (point.inside(imageRectangle))
        { B.push_back(point); }
        point = (cv::Point(contourBoundingRectangles[i].x + contourBoundingRectangles[i].width + 1,
                           contourBoundingRectangles[i].y - 0));
        if (point.inside(imageRectangle))
        { B.push_back(point); }

        point = (cv::Point(contourBoundingRectangles[i].x - 1,
                           contourBoundingRectangles[i].y + contourBoundingRectangles[i].height + 1));
        if (point.inside(imageRectangle))
        { B.push_back(point); }
        point = (cv::Point(contourBoundingRectangles[i].x - 1,
                           contourBoundingRectangles[i].y + contourBoundingRectangles[i].height + 0));
        if (point.inside(imageRectangle))
        { B.push_back(point); }
        point = (cv::Point(contourBoundingRectangles[i].x - 0,
                           contourBoundingRectangles[i].y + contourBoundingRectangles[i].height + 1));
        if (point.inside(imageRectangle))
        { B.push_back(point); }

        point = (cv::Point(contourBoundingRectangles[i].x + contourBoundingRectangles[i].width + 1,
                           contourBoundingRectangles[i].y + contourBoundingRectangles[i].height + 1));
        if (point.inside(imageRectangle))
        { B.push_back(point); }
        point = (cv::Point(contourBoundingRectangles[i].x + contourBoundingRectangles[i].width + 0,
                           contourBoundingRectangles[i].y + contourBoundingRectangles[i].height + 1));
        if (point.inside(imageRectangle))
        { B.push_back(point); }
        point = (cv::Point(contourBoundingRectangles[i].x + contourBoundingRectangles[i].width + 1,
                           contourBoundingRectangles[i].y + contourBoundingRectangles[i].height + 0));
        if (point.inside(imageRectangle))
        { B.push_back(point); }

        std::vector<uchar> B_EB_1;
        std::vector<uchar> B_EB_2;
        std::vector<uchar> B_EB_3;

        //! get median for each color channel
        for (const auto& pointPerChannel : B)
        {
            auto point_color = imageToProc.at<cv::Vec3b>(pointPerChannel);

            B_EB_1.push_back(point_color[0]);
            B_EB_2.push_back(point_color[1]);
            B_EB_3.push_back(point_color[2]);
        }

        //! sorting for further obtaining of medians
        std::sort(B_EB_1.begin(), B_EB_1.end());
        std::sort(B_EB_2.begin(), B_EB_2.end());
        std::sort(B_EB_3.begin(), B_EB_3.end());

        //! background color -- corresponding medians
        cv::Vec3b B_EB;
        B_EB[0] = B_EB_1[B.size() / 2];
        B_EB[1] = B_EB_2[B.size() / 2];
        B_EB[2] = B_EB_3[B.size() / 2];

        //! binarization of area selected by bounding rectangle
        cv::Mat imageInContourBoundingRectangle = imageToProc(contourBoundingRectangles[i]);
        std::vector<cv::Mat> imageInContourBoundingRectangleChannels(
                imageInContourBoundingRectangle.channels());

        cv::split(imageInContourBoundingRectangle, imageInContourBoundingRectangleChannels);

        std::vector<cv::Mat> BW_EB_image(imageToProc.channels());
        BW_EB_image[0] = imageInContourBoundingRectangleChannels[0];
        BW_EB_image[1] = imageInContourBoundingRectangleChannels[1];
        BW_EB_image[2] = imageInContourBoundingRectangleChannels[2];

        //! binarization
        for (int ch = 0; ch < static_cast<int>(BW_EB_image.size()); ++ch)
        {
            if (F_EB[ch] < B_EB[ch])
            {
                BW_EB_image[ch] = 255 * (BW_EB_image[ch] >= F_EB[ch]);
            }
            else
            {
                BW_EB_image[ch] = 255 * (BW_EB_image[ch] < F_EB[ch]);
            }
        }

        //Mat combinationOR = BW_EB_image[0] | BW_EB_image[1] | BW_EB_image[2];
        cv::Mat combinationAND = BW_EB_image[0] & BW_EB_image[1] & BW_EB_image[2];

        //Mat resultImageROI = resultImage(contourBoundingRectangles[i]);
        cv::Mat resultImage2ROI = resultImage2(contourBoundingRectangles[i]);

        //combinationOR.copyTo(resultImageROI);
        combinationAND.copyTo(resultImage2ROI);

    }

    //! morphology operations
    if (useMorphology)
    {
        //cv::dilate(resultImage, resultImage, cv::Mat(), cv::Point(-1, -1), 2);
        //cv::erode(resultImage, resultImage, cv::Mat(), cv::Point(-1, -1), 2);

        cv::dilate(resultImage2, resultImage2, cv::Mat(), cv::Point(-1, -1), 2);
        cv::erode(resultImage2, resultImage2, cv::Mat(), cv::Point(-1, -1), 2);
    }
    outputImage = resultImage2.clone();
}


