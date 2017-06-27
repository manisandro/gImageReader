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

#include "Rotate.hh"

#include "opencv2/imgproc.hpp"

namespace prl
{
void rotate(const cv::Mat& input, cv::Mat& output, const double angle)
{
    if (angle == 90)
    {
        // rotate on 90
        cv::transpose(input, output);
        cv::flip(output, output, 1);
        return;
    }
    else if (angle == 180)
    {
        // rotate on 180
        cv::flip(input, output, -1);
        return;
    }
    else if (angle == 270)
    {
        // rotate on 270
        cv::transpose(input, output);
        cv::flip(output, output, 0);
        return;
    }
    else
    {
        output = input.clone();
        cv::bitwise_not(input, input);

        int len = std::max(output.cols, output.rows);
        cv::Point2f pt(static_cast<float>(len / 2.0), static_cast<float>(len / 2.0));
        cv::Mat r = cv::getRotationMatrix2D(pt, angle, 1.0);

        cv::warpAffine(input, output, r, cv::Size(len, len));

        cv::bitwise_not(input, input);
        cv::bitwise_not(output, output);
    }
}
}
