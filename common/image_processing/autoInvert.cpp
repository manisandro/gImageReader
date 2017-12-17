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

#include "autoInvert.h"

#include <opencv2/core/core.hpp>
#include <opencv2/imgproc/imgproc.hpp>

namespace prl
{

    
    
bool needAutoInvert(const cv::Mat& inputImage)
{
    cv::Mat imageCopy = inputImage.clone();
	if (imageCopy.channels() != 1) 
    {
		cv::cvtColor(inputImage, imageCopy, CV_BGR2GRAY);
	} 

	const cv::Scalar meanValues = cv::mean(imageCopy);
    
    return meanValues[0] >= 128;
}

void invert(const cv::Mat& inputImage, cv::Mat& outputImage)
{
    cv::Mat imageCopy = inputImage.clone();
	if (imageCopy.channels() != 1) 
    {
		cv::cvtColor(inputImage, imageCopy, CV_BGR2GRAY);
	} 
    cv::bitwise_not(imageCopy, outputImage);
}

void autoInvert(const cv::Mat& inputImage, cv::Mat& outputImage)
{
    cv::Mat imageCopy = inputImage.clone();
	if (imageCopy.channels() != 1) 
    {
		cv::cvtColor(inputImage, imageCopy, CV_BGR2GRAY);
	} 
    
	if (needAutoInvert(imageCopy)) 
    {
        cv::bitwise_not(imageCopy, outputImage);
	} 
	else 
    {
		outputImage = imageCopy.clone();
	}
}
}
