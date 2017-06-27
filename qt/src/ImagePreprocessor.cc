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

#include <QImage>

#include <opencv2/core.hpp>

#include "ImagePreprocessor.hh"

#include "cvmatandqimage.hh"
#include "Rotate.hh"
#include "Deblur.hh"
#include "Deskew.hh"
#include "Denoising.hh"

QImage prl::ImagePreprocessor::preprocess(const QImage& src)
{
    cv::Mat image = QtOcv::image2Mat(src);

    //Denoise
    /*cv::Mat denoised_image;
    prl::denoise(image, denoised_image);
    image = denoised_image;*/

    //Try to deskew image
    cv::Mat rotated_image;
    prl::deskew(image, rotated_image);
    //image = rotated_image;
    

    
    return QtOcv::mat2Image(rotated_image);
}
