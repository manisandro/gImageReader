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

#include "Deblur.hh"

#include "opencv2/imgproc.hpp"


void basicDeblur(const cv::Mat& src, cv::Mat& dst)
{
    //http://stackoverflow.com/questions/4993082/how-to-sharpen-an-image-in-opencv
    std::vector<cv::Mat> channels;
    cv::split(src, channels);

    for (auto& channel : channels)
    {
        cv::Mat channelfloat;
        channel.convertTo(channelfloat, CV_32F);
        cv::GaussianBlur(
                channelfloat, channel,
                cv::Size(11, 11),
                10, 30);
        cv::addWeighted(
                channelfloat, 2.0 * 0.75,
                channel, 2.0 * 0.75 - 2.0,
                0.0,
                channel);

        // convert back to 8bits gray scale
        channel.convertTo(channel, CV_8U);
    }

    cv::merge(channels, dst);
}


void prl::deblur(const cv::Mat& src, cv::Mat& dst,
                 DeblurMethod method /*= DeblurMethod::Simple*/)
{
    switch(method)
    {
        case DeblurMethod::Simple:
            basicDeblur(src, dst);
            break;
        default:
            //TODO: implement algorithms
            throw std::runtime_error("Deblur algorithm not implemented yet");
    }
}
