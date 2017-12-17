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

#include "autoCropUtils.h"

#include <algorithm>
#include <iostream>
#include <stdexcept>
#include <vector>

#include <opencv2/highgui/highgui.hpp>
#include <opencv2/imgproc/imgproc.hpp>
#include <opencv2/features2d/features2d.hpp>

#include "imageLibCommon.h"
#include "utils.h"


double angleBetweenLinesInDegree(cv::Point line1Start, cv::Point line1End,
                                 cv::Point line2Start, cv::Point line2End)
{
    double angle1 = atan2(line1Start.y - line1End.y, line1Start.x - line1End.x);
    double angle2 = atan2(line2Start.y - line2End.y, line2Start.x - line2End.x);
    double result = (angle2 - angle1) * 180.0 / 3.14;
    if (result < 0.0)
    {
        result += 360.0;
    }
    if (result > 180.0)
    {
        result = 360.0 - result;
    }
    return result;
}

//! Calculate degree between two vectors
double dotDegree(cv::Point& common, cv::Point& a, cv::Point& b)
{
    cv::Point2d vecA = cv::Point(a.x - common.x, a.y - common.y);
    cv::Point2d vecB = cv::Point(b.x - common.x, b.y - common.y);

    double normA = sqrt(vecA.x * vecA.x + vecA.y * vecA.y);
    double normB = sqrt(vecB.x * vecB.x + vecB.y * vecB.y);

    vecA.x = vecA.x / normA;
    vecA.y = vecA.y / normA;

    vecB.x = vecB.x / normB;
    vecB.y = vecB.y / normB;

    double costheta = (vecA.x * vecB.x + vecA.y * vecB.y);

    double rad = acos(costheta);
    const double PI = CV_PI;
    double degree = rad * 180 / PI;

    return degree;
}

//! Function checks whether polygon rectangle
int CheckRectangle(std::vector<cv::Point>& contour, MarkerRectangle* rectangle)
{
    //! Check count of corners
    if (contour.size() != 4)
    {
        return BAD_RECTANGLE;
    }

    double capaDeg = 25;
    double upperThreshDegree = 180 - capaDeg;
    double lowerThreshDegree = capaDeg;

    double degrees[4];

    std::vector<cv::Point>& corners = contour;

    //! Get degree for each corner
    for (size_t i = 0; i < 4; ++i)
    {
        int next = (i + 1) % 4;
        int prev = (i - 1);
        if (prev == -1)
        {
            prev = 3;
        }
        degrees[i] = dotDegree(corners[i], corners[next], corners[prev]);
    }

    //! Check value of degrees
    int passCount = 0;
    for (size_t i = 0; i < 4; ++i)
    {
        if ((degrees[i] <= upperThreshDegree) &&
            (degrees[i] >= lowerThreshDegree))
        {
            ++passCount;
        }
    }

    if (passCount == 4)
    {
        //! If all degrees are valid then fill output rectangle
        for (size_t i = 0; i < 4; ++i)
        {
            rectangle->outerCorners[i] = cv::Point2f(static_cast<float>(corners[i].x),
                                                     static_cast<float>(corners[i].y));
            rectangle->outerDegrees[i] = degrees[i];
        }
        return GOOD_RECTANGLE;
    }
    else
    {
        // not rectangle
        return BAD_RECTANGLE;
    }
}

/////////////////////////////////////////////////////////////////

bool IsQuadrangularConvex(std::vector<cv::Point>& resultContour)
{
    if (resultContour.empty())
    {
        return false;
    }

    //! Find convex hull of points
    std::vector<cv::Point> convexHullPoints;
    cv::convexHull(cv::Mat(resultContour), convexHullPoints, false);

    return (convexHullPoints.size() == 4);
}

bool findDocumentContour(cv::Mat& source, std::vector<cv::Point2f>& resultContour)
{
    if (source.empty())
    {
        throw std::invalid_argument("Input image for contours detection is empty");
    }

    std::vector<std::vector<cv::Point> > contours;
    std::vector<cv::Vec4i> hierarchy;

    cv::findContours(
            source.clone(), contours, hierarchy,
            CV_RETR_CCOMP, CV_CHAIN_APPROX_SIMPLE,
            cv::Point(0, 0));

    //! Approximate contours to polygons + get bounding rectangles and circles
    std::vector<std::vector<cv::Point> > contoursPolygons(contours.size());
    std::vector<cv::RotatedRect> minRect(contoursPolygons.size());

    float maxRotatedRectArea = 0;
    int maxRotatedRectAreaPos = -1;

    double minArea = static_cast<double>(source.cols * source.rows) * 0.05;

    std::vector<cv::Point> maxCurve;

    for (size_t i = 0; i < contours.size(); ++i)
    {
        //! Approximate contour by polygon
        contoursPolygons[i] = contours[i];
        double approxEps = 1.0;
        while (contoursPolygons[i].size() > 4)
        {
            cv::approxPolyDP(cv::Mat(contours[i]), contoursPolygons[i], approxEps, true);
            approxEps += 1.0;
            contours[i] = contoursPolygons[i];
        }

        if (contoursPolygons[i].size() < 4)
        { continue; }

        double contArea = cv::contourArea(contoursPolygons[i]);

        // Sides check
        double side1 = cv::norm(contoursPolygons[i][0] - contoursPolygons[i][1]);
        double side2 = cv::norm(contoursPolygons[i][1] - contoursPolygons[i][2]);
        double side3 = cv::norm(contoursPolygons[i][2] - contoursPolygons[i][3]);
        double side4 = cv::norm(contoursPolygons[i][3] - contoursPolygons[i][0]);

        if (side1 < side3)
        { std::swap(side1, side3); }
        if (side2 < side4)
        { std::swap(side2, side4); }

        if (side3 / side1 < 0.85)
        { continue; }
        if (side4 / side2 < 0.85)
        { continue; }

        // Sides check - end

        // Angles check
        double angle1 = angleBetweenLinesInDegree(contoursPolygons[i][0], contoursPolygons[i][1],
                                                  contoursPolygons[i][2], contoursPolygons[i][3]);
        double angle2 = angleBetweenLinesInDegree(contoursPolygons[i][1], contoursPolygons[i][2],
                                                  contoursPolygons[i][3], contoursPolygons[i][0]);

        if (angle1 < 160.0)
        { continue; }
        if (angle2 < 160.0)
        { continue; }
        // Angles check - end


        bool isContoursPolygonTooSmall = contArea < minArea;
        bool isContoursPolygonTooBig = contArea > source.cols * source.rows;
        bool hasContoursPolygon4Corners = contoursPolygons[i].size() == 4;
        if (isContoursPolygonTooBig || isContoursPolygonTooSmall || !hasContoursPolygon4Corners)
        {
            continue;
        }

        double contourArea = cv::contourArea(contoursPolygons[i]);

        if (contourArea > minArea)
        {
            minArea = contourArea;
            maxCurve = contoursPolygons[i];
        }
    }

    if (!maxCurve.empty() && IsQuadrangularConvex(maxCurve))
    {
        for (size_t i = 0; i < 4; ++i)
        {
            resultContour.push_back(
                    cv::Point2f(
                            static_cast<float>(maxCurve[i].x),
                            static_cast<float>(maxCurve[i].y)
                    )
            );
        }

        return true;
    }

    return false;
}

bool isQuadrangle(const std::vector<cv::Point>& contour)
{
    return contour.size() == 4;
}

bool cropVerticesOrdering(std::vector<cv::Point2f>& contour)
{
    if (contour.empty())
    {
        throw std::invalid_argument("Contour for ordering is empty");
    }

    //! This should be 4 for rectangles, but it allows to be extended to more complex cases
    int verticesNumber = static_cast<int>(contour.size());
    if (verticesNumber != 4)
    {
        throw std::invalid_argument("Points number in input contour isn't equal 4");
    }

    //! Find convex hull of points
    std::vector<cv::Point2f> points;
    points = contour;

    // indices of convex vertices
    std::vector<int> indices;
    cv::convexHull(points, indices, false);

    //! Find top left point, as starting point. This is the nearest to (0.0) point
    int minDistanceIdx = 0;
    double minDistanceSqr =
            contour[indices[0]].x * contour[indices[0]].x +
            contour[indices[0]].y * contour[indices[0]].y;

    for (int i = 1; i < verticesNumber; ++i)
    {
        double distanceSqr =
                contour[indices[i]].x * contour[indices[i]].x +
                contour[indices[i]].y * contour[indices[i]].y;

        if (distanceSqr < minDistanceSqr)
        {
            minDistanceIdx = i;
            minDistanceSqr = distanceSqr;
        }
    }

    //! Now starting point index in source array is in indices[minDistanceIdx]
    //! convex hull vertices are sorted clockwise in indices array, so transfer them to result
    //! beginning from starting point to the end of array...
    std::vector<cv::Point2f> ret(verticesNumber);
    int idx = 0;
    for (int i = minDistanceIdx; i < verticesNumber; ++i, ++idx)
    {
        ret[idx] = cv::Point2f(contour[indices[i]].x, contour[indices[i]].y);
    }

    //! ... and from the begin of array up to starting point
    for (int i = 0; i < minDistanceIdx; ++i, ++idx)
    {
        ret[idx] = cv::Point2f(contour[indices[i]].x, contour[indices[i]].y);
    }

    contour = ret;

    return true;
}

void scaleContour(std::vector<cv::Point2f>& contour, cv::Size& fromImageSize, cv::Size& toImageSize)
{
    if (contour.empty())
    {
        throw std::invalid_argument("Contour for scaling is empty");
    }

    float xScale = static_cast<float>(toImageSize.width) /
                   static_cast<float>(fromImageSize.width);
    float yScale = static_cast<float>(toImageSize.height) /
                   static_cast<float>(fromImageSize.height);

    for (size_t i = 0; i < contour.size(); ++i)
    {
        contour[i].x *= xScale;
        contour[i].y *= yScale;
    }
}

bool intersection(cv::Point2f o1, cv::Point2f p1, cv::Point2f o2, cv::Point2f p2,
                  cv::Point2f& r)
{
    cv::Point2f x = o2 - o1;
    cv::Point2f d1 = p1 - o1;
    cv::Point2f d2 = p2 - o2;

    float cross = d1.x * d2.y - d1.y * d2.x;
    if (prl::eq_d(cross, 0.0))
    {
        return false;
    }

    double t1 = (x.x * d2.y - x.y * d2.x) / cross;
    r = o1 + d1 * t1;
    return true;
}