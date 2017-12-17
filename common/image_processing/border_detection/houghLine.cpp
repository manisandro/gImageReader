#include "houghLine.h"

#include <algorithm>
#include <cmath>
#include <stdexcept>
#include <vector>

#include <opencv2/highgui/highgui.hpp>
#include <opencv2/imgproc/imgproc.hpp>

#include "autoCropUtils.h"
//#include "binarizeLocalOtsu.h"
#include "imageLibCommon.h"
#include "utils.h"

double distanceToLine(cv::Point line_start, cv::Point line_end, cv::Point point)
{
    double normalLength = std::hypot(line_end.x - line_start.x, line_end.y - line_start.y);
    double distance =
            (double) (
                    (point.x - line_start.x) * (line_end.y - line_start.y) -
                    (point.y - line_start.y) * (line_end.x - line_start.x)
            ) / normalLength;
    return std::fabs(distance);
}

void deleteSimilarLines(
        std::vector<cv::Vec2f>& inputLines,
        double minDist,
        const double stepDist,
        const int maxLines)
{
    std::vector<cv::Vec2f> tmpLines;
    std::vector<char> used(inputLines.size(), false);
    while (static_cast<int>(inputLines.size()) > maxLines)
    {
        std::fill(used.begin(), used.end(), false);
        tmpLines.clear();

        for (size_t i = 0; i < inputLines.size(); ++i)
        {
            if (used[i])
            {
                continue;
            }
            used[i] = true;
            tmpLines.push_back(inputLines[i]);
            auto vec1 = fromVec2f(inputLines[i]);
            for (size_t j = i + 1; j < inputLines.size(); ++j)
            {
                if (used[j])
                {
                    continue;
                }
                auto vec2 = fromVec2f(inputLines[j]);

                //Calculate minimal distance between two line segments
                double dist =
                        std::min(
                                std::min(
                                        distanceToLine(vec1.first, vec1.second, vec2.first),
                                        distanceToLine(vec1.first, vec1.second, vec2.second)
                                ),
                                std::min(
                                        distanceToLine(vec2.first, vec2.second, vec1.first),
                                        distanceToLine(vec2.first, vec2.second, vec1.second)
                                )
                        );

                //If distance < minDist, we delete this line
                if (dist < minDist)
                {
                    used[j] = true;
                }
            }
        }

        if (tmpLines.size() < 4)
        {
            inputLines.resize(maxLines);
            break;
        }
        inputLines = tmpLines;
        minDist += stepDist;
    }
}

std::vector<cv::Point2f> findMaxValidContour(const std::vector<cv::Vec2f>& lines, cv::Size size)
{
    double maxFoundedArea = 0.0;
    std::vector<cv::Point2f> resultVec;
    for (size_t i = 0; i < lines.size(); ++i)
    {
        for (size_t j = 0; j < lines.size(); ++j)
        {
            if (i == j)
            {
                continue;
            }
            for (size_t k = 0; k < lines.size(); ++k)
            {
                if (i == k || j == k)
                {
                    continue;
                }
                for (size_t m = 0; m < lines.size(); ++m)
                {
                    if (i == m || j == m || k == m)
                    {
                        continue;
                    }

                    //Here we check lines for intersection.
                    std::vector<cv::Point2f> points(4);
                    std::vector<std::pair<cv::Point2f, cv::Point2f> > curLines;
                    curLines.push_back(fromVec2f(lines[i]));
                    curLines.push_back(fromVec2f(lines[j]));
                    curLines.push_back(fromVec2f(lines[k]));
                    curLines.push_back(fromVec2f(lines[m]));
                    curLines.push_back(curLines[0]);

                    bool allIntersect = true;
                    for (size_t ind = 1; ind < curLines.size(); ++ind)
                    {
                        bool areLinesIntersected =
                                intersection(
                                        curLines[ind].first, curLines[ind].second,
                                        curLines[ind - 1].first, curLines[ind - 1].second,
                                        points[ind - 1]
                                );

                        if (!areLinesIntersected)
                        {
                            allIntersect = false;
                            break;
                        }
                    }
                    if (!allIntersect)
                    {
                        continue;
                    }


                    bool validCoord = false;
                    for (const auto& val : points)
                    {
                        //Check point's coordinates
                        //Maybe be wrong, if corner is outside
                        if (val.x < 0 || val.x > size.width || val.y < 0 || val.y > size.height)
                        {
                            validCoord = true;
                            break;
                        }
                    }
                    if (validCoord)
                    {
                        continue;
                    }

                    //Find the largest convex quadrangular area
                    cv::Mat temp(points);
                    double tempArea = cv::contourArea(temp);
                    if (cv::isContourConvex(temp) && IsQuadrangularConvex(points) &&
                        tempArea > maxFoundedArea)
                    {
                        maxFoundedArea = tempArea;
                        resultVec = points;
                    }
                }
            }
        }
    }

    return resultVec;
}

bool prl::findHoughLineContour(cv::Mat& inputImage,
                          std::vector<cv::Point>& resultContour)
{
    if (inputImage.empty())
    {
        throw std::invalid_argument("Input image is empty");
    }

    cv::Mat imageToProc = inputImage.clone();

    // TODO:-1) Resize image for faster border detection
    //       0) Find most informative channel
    //       1) Median blur 6 times with  kernel sizes 3 and 5 - done
    //       2) Use binarization (LocalOtsu) - done
    //       3) Canny - done
    //       4) Dilate - done
    //       5) HoughLine - done
    //       6) Find largest area - done
    //       7) Add heuristics for angles


    // Median blur with two kernels
    for (size_t i = 0; i < 3; ++i)
    {
        cv::medianBlur(imageToProc, imageToProc, 3);
    }
    for (size_t i = 0; i < 3; ++i)
    {
        cv::medianBlur(imageToProc, imageToProc, 5);
    }

    cv::imwrite("after_blur.jpg", imageToProc);

    // Binarization
    //cv::Mat binarizedImage;
    //prl::binarizeLocalOtsu(imageToProc, binarizedImage);

    // Canny
    cv::Mat resultCanny;
    double upper = 50;
    double lower = 25;
    cv::Canny(imageToProc, resultCanny, lower, upper);

    cv::imwrite("after_canny.jpg", resultCanny);

    // Dilate
    //cv::dilate(resultCanny, resultCanny, cv::Mat(), cv::Point(-1, -1), 2);

    cv::imwrite("after_dilate.jpg", resultCanny);

    std::vector<cv::Vec2f> lines;
    const int thresholdHough = 50;
    cv::HoughLines(resultCanny, lines, 1, CV_PI / 180.0, thresholdHough, 0, 0);

    //Can't create quadrilateral with less than 4 lines
    if (lines.size() < 4)
    {
        return false;
    }

    //Delete similar lines
    deleteSimilarLines(lines, 5.0, 5.0, 20);

    //Bruteforce lines. Try to find 4 lines for building the largest area
    auto resultVec = findMaxValidContour(lines, resultCanny.size());

    if (resultVec.empty())
    {
        return false;
    }

    for(const auto& vertice : resultVec)
    {
        resultContour.push_back({static_cast<int>(vertice.x), static_cast<int>(vertice.y)});
    }

    //resultContour = std::vector<cv::Point>(resultVec.begin(), resultVec.end());

    return true;
}

