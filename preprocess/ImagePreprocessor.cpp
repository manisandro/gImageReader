//
// Created by zamazan4ik on 14.06.17.
//

#include <QImage>

#include <opencv2/core.hpp>

#include "ImagePreprocessor.hpp"

#include "cvmatandqimage.h"
#include "Rotate.hpp"
#include "Deblur.hpp"
#include "Deskew.hpp"
#include "Denoising.hpp"

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
