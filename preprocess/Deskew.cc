/* -*- Mode: C++; indent-tabs-mode: t; c-basic-offset: 4; tab-width: 4 -*-  */
/*
 * Config.hh
 * Copyright (C) 2017 Alexander Zaitsev <zamazan4ik@tut.by>
 *
 * gImageReader is free software: you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * gImageReader is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "Deskew.hh"

#include "Rotate.hh"
#include "FormatConvert.hh"

#include "opencv2/imgproc.hpp"
#include "opencv2/highgui.hpp"


#ifdef _MSC_VER
# pragma warning(push)
# pragma warning(disable : 4305)
# pragma warning(disable : 4005)
#endif // _MSC_VER

#include <leptonica/allheaders.h>

#ifdef _MSC_VER
# pragma warning(pop)
#endif // _MSC_VER

#include <algorithm>
#include <functional>
#include <cctype>
#include <list>
#include <map>

#ifndef M_PI
#ifdef CV_PI
#define M_PI CV_PI
#else
#define M_PI 3.141592653
#endif
#endif // !M_PI



double FindOrientation(const cv::Mat& input)
{
    PIX* pix = prl::ImgOpenCvToLepton(input);
    pix = pixConvertTo1(pix, 200);
    if (!pix)
    {
        return 0;
    }

    l_int32 iOrientation = 0;
    {
        l_float32 fUpConf = 10.0;
        l_float32 fLeftConf = 10.0;
        if (pixOrientDetect(pix, &fUpConf, &fLeftConf, 0, 0) != 0)
        {
            if (pix)
            {
                pixDestroy(&pix);
            }

            return 0;
        }

        if (makeOrientDecision(fUpConf, fLeftConf, 0.0, 0.0, &iOrientation, 0) != 0)
        {
            if (pix)
            {
                pixDestroy(&pix);
            }

            return 0;
        }
    }

    double angle = 0;
    if (iOrientation == L_TEXT_ORIENT_UP)
    {
        angle = 0.0;
    }
    else if (iOrientation == L_TEXT_ORIENT_LEFT)
    {
        angle = 90.0;
    }
    else if (iOrientation == L_TEXT_ORIENT_DOWN)
    {
        angle = 180.0;
    }
    else if (iOrientation == L_TEXT_ORIENT_RIGHT)
    {
        angle = 270.0;
    }
    else
    { // if (iOrientation == L_TEXT_ORIENT_UNKNOWN)
        angle = 0.0;
    }

    pixDestroy(&pix);

    return angle;
}

bool compare_pairs(const std::pair<double, int>& p1, const std::pair<double, int>& p2)
{
    return p1.second < p2.second;
}

bool eq_d(const double v1, const double v2, const double delta)
{
    return std::abs(v1 - v2) <= delta;

}

double FindAngle(const cv::Mat& input_orig)
{
    // AA: we need black-&-white image here even if none of threshold algorithms were called before
    //cv::adaptiveThreshold(input, input, 255, CV_ADAPTIVE_THRESH_GAUSSIAN_C, CV_THRESH_BINARY, 15, 5);
    cv::Mat input = input_orig.clone();

    cv::Size imgSize = input.size();
    cv::bitwise_not(input, input);
    std::vector<cv::Vec4i> lines;
    cv::HoughLinesP(input, lines, 1, CV_PI / 180, 100, imgSize.width / 8.f, 20);
    cv::Mat disp_lines(imgSize, CV_8UC1, cv::Scalar(0, 0, 0));

    const size_t nb_lines = static_cast<size_t>(lines.size());
    if (nb_lines == 0)
    {
        return 0.0;
    }

    std::vector<double> cv_angles = std::vector<double>(nb_lines);

    for (size_t i = 0; i < nb_lines; ++i)
    {
        cv::line(
                disp_lines,
                cv::Point(lines[i][0], lines[i][1]),
                cv::Point(lines[i][2], lines[i][3]),
                cv::Scalar(255, 0, 0));

        cv_angles[i] = atan2(
                (double) lines[i][3] - lines[i][1],
                (double) lines[i][2] - lines[i][0]);
    }

    const double delta = 0.01; //difference is less than 1 deg.
    std::list<std::pair<double, int> > t_diff;

    for (auto angle : cv_angles)
    {
        bool found = false;
        // bypass list
        for (auto& elem : t_diff)
        {
            if (eq_d(angle, elem.first, delta))
            {
                elem.second++;
                found = true;
                break;
            }
        }

        if (!found)
        {
            t_diff.push_back(std::make_pair(angle, 0));
        }
    }

    std::pair<double, int> max_elem =
            *(std::max_element(t_diff.begin(), t_diff.end(), compare_pairs));

    const double cv_angle = max_elem.first * 180 / M_PI;

    cv::bitwise_not(input, input);

    return cv_angle;
}

bool prl::deskew(cv::Mat& inputImage, cv::Mat& deskewedImage)
{
    cv::Mat processingImage;

    if (inputImage.channels() != 1)
    {
        cv::cvtColor(inputImage, processingImage, CV_BGR2GRAY);
    }
    else
    {
        processingImage = inputImage.clone();
    }

    //TODO: Maybe here we should use something more efficient than global Otsu?
    cv::threshold(processingImage, processingImage, 128, 255, CV_THRESH_BINARY | CV_THRESH_OTSU);

    double angle = FindAngle(processingImage);
    
    //Check for zero angle and some strange values for double: NaN, inf, -inf
    if ((angle != 0) && (angle > -std::numeric_limits<double>::max() && angle < std::numeric_limits<double>::max()))
    {
        //TODO: Can we rewrite it without copies?
        cv::Mat rotatedImage;
        prl::rotate(inputImage, rotatedImage, angle);
        deskewedImage = rotatedImage.clone();
    }
    else
    {
        deskewedImage = inputImage.clone();
    }

    angle = FindOrientation(deskewedImage);
    
    if (angle != 0)
    {
        cv::Mat rotatedImage;
        prl::rotate(deskewedImage, rotatedImage, angle);
        deskewedImage = rotatedImage.clone();
    }

    return !deskewedImage.empty();
}
