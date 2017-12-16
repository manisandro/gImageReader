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

#include "binarizeCOCOCLUST.h"

#include <algorithm>
#include <cmath>
#include <functional>
#include <map>
#include <memory>
#include <stdexcept>
#include <vector>

#include <opencv2/highgui/highgui.hpp>
#include <opencv2/imgproc/imgproc.hpp>
#include <opencv2/features2d/features2d.hpp>

#include "imageLibCommon.h"

void prl::binarizeCOCOCLUST(cv::Mat& inputImage, cv::Mat& outputImage, const float T_S,
                         double CLAHEClipLimit,
                         int GaussianBlurKernelSize,
                         double CannyUpperThresholdCoeff,
                         double CannyLowerThresholdCoeff,
                         int CannyMorphIters)
{
    // input image must be not empty
    if (inputImage.empty())
    {
        throw std::invalid_argument("binarizeCOCOCLUST: Input image for binarization is empty");
    }
    // we work with color images
    if (inputImage.type() != CV_8UC3 && inputImage.type() != CV_8UC1)
    {
        throw std::invalid_argument("binarizeCOCOCLUST: Invalid type of image for binarization (required 8 or 24 bits per pixel)");
    }

    if (T_S <= 0)
    {
        throw std::invalid_argument("binarizeCOCOCLUST: Cluster distance threshold should be greater than 0");
    }

    cv::Mat imageToProc = inputImage.clone();
    cv::Mat imageSelectedColorSpace;

    //! Processing for gray scaled image
    if (inputImage.type() == CV_8UC3)
    {
        cvtColor(inputImage, imageSelectedColorSpace, CV_RGB2GRAY);
    }
    else
    {
        imageSelectedColorSpace = inputImage.clone();
    }

    const int intensityChannelNo = 0;

    //! Intensity channel extraction
    {
        std::vector<cv::Mat> channels(imageSelectedColorSpace.channels());
        split(imageSelectedColorSpace, channels);

        imageToProc = channels[intensityChannelNo].clone();
    }

    //! Enhance local contrast if it is required
    if (CLAHEClipLimit > 0)
    {
        EnhanceLocalContrastByCLAHE(imageToProc, imageToProc, CLAHEClipLimit, true);
    }

    cv::Mat resultCanny;

    //! Detect edges by using Canny edge detector
    CannyEdgeDetection(imageToProc, resultCanny,
                       GaussianBlurKernelSize,
                       CannyUpperThresholdCoeff, CannyLowerThresholdCoeff, CannyMorphIters);

    cv::dilate(resultCanny, resultCanny, cv::Mat(), cv::Point(-1, -1), 3);
    //erode(resultCanny, resultCanny, Mat());

    //! We should have corresponding channels count for processing
    if (imageSelectedColorSpace.channels() != 3)
    {
        cv::cvtColor(imageSelectedColorSpace, imageSelectedColorSpace, CV_GRAY2BGR);
    }

    // Work copy of contours set
    std::vector<std::vector<cv::Point>> contours;
    // full contours set
    std::vector<std::vector<cv::Point>> contoursAll;

    //! Find contours and remove not required
    {
        std::vector<cv::Vec4i> hierarchy;

        cv::findContours(resultCanny, contoursAll, hierarchy,
                         CV_RETR_EXTERNAL, CV_CHAIN_APPROX_SIMPLE, cv::Point(0, 0));

        contours = contoursAll;

        RemoveChildrenContours(contours, hierarchy);

    }

    // Set of mean points for contours parts
    std::vector<std::vector<cv::Point2f>> contoursMeanPoints;
    // Set of normal vectors to contours in mean points
    std::vector<std::vector<cv::Point2f>> contoursNormalVectors;

    //! Set width of range for mean calculation
    int S = 5;
    int halfS = (S - 1) / 2;
    double backS = 1.0 / (static_cast<double>(S));

    const float PI = 3.141592653f;
    const float PI_2 = PI / 2.0f;

    //! Set constants for normal vector  normalization
    float normalVectorCoeffsMatrix[4] = {cosf(PI_2), -sinf(PI_2), sinf(PI_2), cosf(PI_2)};

    //! Set count of normal vectors for each contour
    const int normalsNo = 6;

    // color prototypes
    std::vector<cv::Vec3f> CP;
    // this map stores contour number and corresponding set of external normal vectors
    // for the contour
    std::map<size_t, std::vector<std::vector<cv::Point2f>>> contourNoToNormPVectorsMap;

    {
        //for (vector<Point> &contour : contours) {
        for (size_t contourNo = 0; contourNo < contours.size(); ++contourNo)
        {
            // reference to current contour
            std::vector<cv::Point>& contour = contours[contourNo];

            bool isContourTooSmall = contour.size() <= static_cast<size_t>(S);
            if (isContourTooSmall || contour.size() <= static_cast<size_t>(normalsNo))
            {
                continue;
            }

            //! Calculate current contour mean points
            contoursMeanPoints.emplace_back(std::vector<cv::Point2f>());
            std::vector<cv::Point2f>& currentContourMeanPoints = contoursMeanPoints.back();
            {
                std::vector<cv::Point2f> tempContour(contour.size() + (halfS << 1));

                //! Extend contour by copying points from begin to end of array
                //! and from end to begin
                std::copy(contour.end() - halfS, contour.end(), tempContour.begin());
                std::copy(contour.begin(), contour.end(), tempContour.begin() + halfS);
                std::copy(contour.begin(), contour.begin() + halfS,
                     tempContour.begin() + contour.size() + halfS);

                cv::Point2f meanPoint;

                //! Get part of contour
                for (int i = 0; i < S; ++i)
                {
                    meanPoint += tempContour[i];
                }
                currentContourMeanPoints.push_back(meanPoint);

                for (size_t i = halfS; i < contour.size(); ++i)
                {
                    meanPoint -= tempContour[i - halfS];
                    meanPoint += tempContour[i + halfS + 1];
                    currentContourMeanPoints.push_back(meanPoint);
                }

                for (auto& currentContour : currentContourMeanPoints)
                {
                    currentContour *= backS;
                }

                //! Remove duplicate points
                auto it = std::unique(currentContourMeanPoints.begin(), currentContourMeanPoints.end());
                currentContourMeanPoints.resize(std::distance(currentContourMeanPoints.begin(), it));
            }

            //! Calculate normal vectors
            contoursNormalVectors.emplace_back(std::vector<cv::Point2f>());
            std::vector<cv::Point2f>& currentContourNormalVectors = contoursNormalVectors.back();
            {
                std::vector<cv::Point2f> tempContourMeanPoints(currentContourMeanPoints.size() + 2);

                //! Extend contour by copying points from begin to end of array
                //! and from end to begin
                std::copy(currentContourMeanPoints.end() - 1, currentContourMeanPoints.end(),
                          tempContourMeanPoints.begin());
                std::copy(currentContourMeanPoints.begin(), currentContourMeanPoints.end(),
                          tempContourMeanPoints.begin() + 1);
                std::copy(currentContourMeanPoints.begin(), currentContourMeanPoints.begin() + 1,
                          tempContourMeanPoints.begin() + currentContourMeanPoints.size() + 1);

                cv::Point2f normalVector;

                //! Calculate normals
                for (size_t i = 0; i < tempContourMeanPoints.size() - 2; ++i)
                {
                    cv::Point2f vector1 = tempContourMeanPoints[i + 1] - tempContourMeanPoints[i + 0];
                    vector1 *= 1.0 / norm(vector1);
                    cv::Point2f vector2 = tempContourMeanPoints[i + 2] - tempContourMeanPoints[i + 1];
                    vector2 *= 1.0 / norm(vector2);
                    normalVector = vector1 + vector2;
                    normalVector *= 0.5;
                    normalVector = cv::Point2f(
                            normalVectorCoeffsMatrix[0] * normalVector.x +
                            normalVectorCoeffsMatrix[1] * normalVector.y,
                            normalVectorCoeffsMatrix[2] * normalVector.x +
                            normalVectorCoeffsMatrix[3] * normalVector.y);
                    currentContourNormalVectors.push_back(normalVector);
                }
            }

            //! Calculate color prototypes
            {
                const size_t nVectorLength = 5;
                cv::Rect imageRectangle(0, 0,
                                        imageSelectedColorSpace.cols, imageSelectedColorSpace.rows);
                for (size_t i = 0; i < currentContourNormalVectors.size(); ++i)
                {

                    bool isVectorsNumberSufficientlyLarge =
                            (currentContourNormalVectors.size() > normalsNo);
                    bool isCurrentVectorNumberProportionalDistanceBetweenNormals =
                            ((i % (currentContourNormalVectors.size() / normalsNo)) == 0);
                    if (isVectorsNumberSufficientlyLarge &&
                        !isCurrentVectorNumberProportionalDistanceBetweenNormals)
                    {
                        continue;
                    }

                    // colors in external normal vectors
                    std::vector<cv::Vec3f> colorsP(nVectorLength);
                    // colors in internal normal vectors
                    std::vector<cv::Vec3f> colorsM(nVectorLength);

                    {
                        // points composing external normal vector
                        std::vector<cv::Point2f> vectP(nVectorLength);
                        // points composing internal normal vector
                        std::vector<cv::Point2f> vectM(nVectorLength);

                        //! Calculate point colors for internal and external vectors
                        size_t j = 0;
                        for (j = 0; j < nVectorLength; ++j)
                        {

                            //! Get external vector
                            vectP[j] = currentContourMeanPoints[i] +
                                       currentContourNormalVectors[i] * static_cast<float>(j + 1);
                            //! Get internal vector
                            vectM[j] = currentContourMeanPoints[i] -
                                       currentContourNormalVectors[i] * static_cast<float>(j + 1);

                            //! Test points of vector are not placed in processed image
                            bool isImageRectContainsVectP = imageRectangle.contains(vectP[j]);
                            bool isImageRectContainsVectM = imageRectangle.contains(vectM[j]);
                            if (!isImageRectContainsVectP || !isImageRectContainsVectM)
                            {
                                break;
                            }

                            //! Get colors in corresponding points
                            colorsP[j] = imageSelectedColorSpace.at<cv::Vec3b>(vectP[j]);
                            colorsM[j] = imageSelectedColorSpace.at<cv::Vec3b>(vectM[j]);

                        }

                        //! Store external vector for further usage
                        if (j == nVectorLength)
                        {
                            contourNoToNormPVectorsMap[contourNo].push_back(vectP);
                        }
                    }

                    //! Obtain color medians for colors_p and 
                    //! colors_m and store them to CP
                    //! for each color channel separately

                    cv::Vec3f colorsPMedian, colorsMMedian;
                    std::vector<float> colorChannel0P, colorChannel0M;
                    std::vector<float> colorChannel1P, colorChannel1M;
                    std::vector<float> colorChannel2P, colorChannel2M;

                    for (size_t i = 0; i < nVectorLength; ++i)
                    {
                        colorChannel0P.push_back(colorsP[i][0]);
                        colorChannel0M.push_back(colorsM[i][0]);
                        colorChannel1P.push_back(colorsP[i][1]);
                        colorChannel1M.push_back(colorsM[i][1]);
                        colorChannel2P.push_back(colorsP[i][2]);
                        colorChannel2M.push_back(colorsM[i][2]);
                    }

                    std::sort(colorChannel0P.begin(), colorChannel0P.end());
                    std::sort(colorChannel0M.begin(), colorChannel0M.end());
                    std::sort(colorChannel1P.begin(), colorChannel1P.end());
                    std::sort(colorChannel1M.begin(), colorChannel1M.end());
                    std::sort(colorChannel2P.begin(), colorChannel2P.end());
                    std::sort(colorChannel2M.begin(), colorChannel2M.end());

                    colorsPMedian[0] = colorChannel0P[(nVectorLength - 1) / 2];
                    colorsMMedian[0] = colorChannel0M[(nVectorLength - 1) / 2];
                    colorsPMedian[1] = colorChannel1P[(nVectorLength - 1) / 2];
                    colorsMMedian[1] = colorChannel1M[(nVectorLength - 1) / 2];
                    colorsPMedian[2] = colorChannel2P[(nVectorLength - 1) / 2];
                    colorsMMedian[2] = colorChannel2M[(nVectorLength - 1) / 2];

                    //! Store median to color prototypes array
                    CP.push_back(colorsPMedian);
                    CP.push_back(colorsMMedian);

                }

            }

        } // for (vector<Point> &contour : contours)
    }

    //! Binarize image:
    cv::Mat binarized(imageSelectedColorSpace.size(), CV_8UC1);
    {
        //! Set rectangle of processed image
        cv::Rect imageRectangle(0, 0, imageToProc.cols, imageToProc.rows);

        //! Set default background
        binarized.setTo(255);

        for (size_t contourNo = 0; contourNo < contours.size(); ++contourNo)
        {
            std::vector<cv::Point>& contour = contours[contourNo];

            //! Get foreground color value
            float FG = 0;
            {
                int N_E = static_cast<int>(contour.size());

                for (size_t pointNo = 0; pointNo < contour.size(); ++pointNo)
                {
                    if (!imageRectangle.contains(contour[pointNo]))
                    {
                        continue;
                    }

                    FG += imageSelectedColorSpace.at<cv::Vec3b>(
                            contour[pointNo])[intensityChannelNo];
                }

                FG /= N_E;
            }

            //! Get background color value
            float BG = 0;
            {
                std::vector<float> intensityValues;

                std::vector<std::vector<cv::Point2f>>& backgroundVectors =
                        contourNoToNormPVectorsMap[contourNo];

                for (const auto& vector : backgroundVectors)
                {
                    for (const auto& vectorPoint : vector)
                    {
                        intensityValues.push_back(
                                imageSelectedColorSpace.at<cv::Vec3b>(vectorPoint)[intensityChannelNo]);
                    }
                }

                if (intensityValues.empty())
                {
                    continue;
                }

                std::sort(intensityValues.begin(), intensityValues.end());

                BG = intensityValues[intensityValues.size() / 2];
            }

            //! Binarization in contour bounding rectangle
            {
                cv::Rect contourBoundingRectangle = boundingRect(contour);

                std::vector<cv::Mat> imageChannels(imageSelectedColorSpace.channels());
                cv::split(imageSelectedColorSpace, imageChannels);

                cv::Mat imageInContourBoundingRect =
                        imageChannels[intensityChannelNo](contourBoundingRectangle);

                bool isContourClockwised = (cv::contourArea(contour, true) > 0);

                if (FG > BG)
                {
                    if (isContourClockwised)
                    {
                        imageInContourBoundingRect = 255 * (imageInContourBoundingRect >= FG);
                    }
                    else
                    {
                        imageInContourBoundingRect = 255 * (imageInContourBoundingRect < FG);
                    }
                }
                else
                {
                    if (!isContourClockwised)
                    {
                        imageInContourBoundingRect = 255 * (imageInContourBoundingRect >= FG);
                    }
                    else
                    {
                        imageInContourBoundingRect = 255 * (imageInContourBoundingRect < FG);
                    }
                }

                imageInContourBoundingRect.copyTo(binarized(contourBoundingRectangle));

            }

        }

        outputImage = binarized.clone();
    }
}

