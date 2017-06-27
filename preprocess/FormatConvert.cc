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

#include "FormatConvert.hh"

#if defined(WIN32) || defined(_MSC_VER)
#include <windows.h>
#else
#include "Compatibility.hh"
#endif


#include "leptonica/allheaders.h"


#define DIBBYTESPERLINE(bits) (((bits) + 31) / 32 * 4)


int hmod(const int num, const int denom)
{
    CV_Assert(denom != 0);
    if (num % denom == 0)
    {
        return num / denom;
    }
    else
    {
        return num / denom + 1;
    }
}

static PIX* _CreatePIX(const BITMAPINFO& Bmi2, unsigned char* pBytes)
{
    if (Bmi2.bmiHeader.biCompression != 0 || Bmi2.bmiHeader.biBitCount != 1)
    {
        CV_Assert(Bmi2.bmiHeader.biCompression == 0);
        CV_Assert(Bmi2.bmiHeader.biBitCount == 1);
        return nullptr;
    }

    PIX* pix = pixCreate(Bmi2.bmiHeader.biWidth, abs(Bmi2.bmiHeader.biHeight), 1);
    if (!pix)
    {
        return nullptr;
    }

    pixSetXRes(pix, (l_int32) ((l_float32) Bmi2.bmiHeader.biXPelsPerMeter / 39.37 + 0.5)); /* to ppi */
    pixSetYRes(pix, (l_int32) ((l_float32) Bmi2.bmiHeader.biYPelsPerMeter / 39.37 + 0.5)); /* to ppi */

    {
        PIXCMAP* cmap = pixcmapCreate(1);
        memcpy(cmap->array, &Bmi2.bmiColors[0], 2 * sizeof(RGBA_QUAD));
        cmap->n = 2;
        pixSetColormap(pix, cmap);
    }

    {
        const int pixBpl = 4 * pixGetWpl(pix);

        unsigned char* pDest = (unsigned char*) pixGetData(pix);
        unsigned char* pSrc = pBytes;
        int biDestInc = pixBpl;
        int biBPLSrc = DIBBYTESPERLINE(Bmi2.bmiHeader.biWidth * 1);
        if (Bmi2.bmiHeader.biHeight > 0)
        {
            pDest = (unsigned char*) pixGetData(pix) + pixBpl * (Bmi2.bmiHeader.biHeight - 1);
            biDestInc = -pixBpl;
        }

        for (int i = 0; i < abs(Bmi2.bmiHeader.biHeight); i++)
        {
            memcpy(pDest, pSrc, biBPLSrc);
            pDest += biDestInc;
            pSrc += biBPLSrc;
        }
    }

    pixEndianByteSwap(pix);

/* —------------------------------------------—
* The bmp colormap determines the values of black
* and white pixels for binary in the following way:
* if black = 1 (255), white = 0
* 255, 255, 255, 0, 0, 0, 0, 0
* if black = 0, white = 1 (255)
* 0, 0, 0, 0, 255, 255, 255, 0
* We have no need for a 1 bpp pix with a colormap!
* —------------------------------------------— */
    {
        PIX* pPixTemp;
        if ((pPixTemp = pixRemoveColormap(pix, REMOVE_CMAP_BASED_ON_SRC)) == nullptr)
        {
            pixDestroy(&pix);
            return nullptr;
        }
        pixDestroy(&pix);
        pix = pPixTemp;
    }

    return pix;
}

PIX* prl::ImgOpenCvToLepton(const cv::Mat src)
{
    PIX* pix = nullptr;

    unsigned char* buffer = new unsigned char[sizeof(BITMAPINFOHEADER) + 2 * sizeof(RGBQUAD)];
    BITMAPINFO* bmi = (BITMAPINFO*) buffer;

    bmi->bmiHeader.biSize = sizeof(BITMAPINFOHEADER) + 2 * sizeof(RGBQUAD);
    bmi->bmiHeader.biWidth = src.size().width;
    bmi->bmiHeader.biHeight = src.size().height;
    bmi->bmiHeader.biPlanes = 1;
    bmi->bmiHeader.biBitCount = 1;
    bmi->bmiHeader.biCompression = 0;
    bmi->bmiHeader.biSizeImage = 0;
    bmi->bmiHeader.biXPelsPerMeter = 0;
    bmi->bmiHeader.biYPelsPerMeter = 0;
    bmi->bmiHeader.biClrUsed = 0;
    bmi->bmiHeader.biClrImportant = 0;

    {
        RGBQUAD* colormap = (RGBQUAD*) (buffer + sizeof(BITMAPINFOHEADER));
        colormap[0].rgbBlue = 0xFF;
        colormap[0].rgbGreen = 0xFF;
        colormap[0].rgbRed = 0xFF;
        colormap[1].rgbBlue = 0x00;
        colormap[1].rgbGreen = 0x00;
        colormap[1].rgbRed = 0x00;
    }

    int psize = hmod(src.size().width, 32) * 4 * src.size().height;
    unsigned char* bytes = new unsigned char[psize];
    memset(bytes, 0, psize);
    pix = _CreatePIX(*bmi, bytes);
    delete[] bytes;
    bytes = (unsigned char*) (pix->data);

    unsigned char* blockPointer = &bytes[3];
    int blockDelta = 4;

    for (int j = 0; j < src.size().height; j++)
    {
        for (int i = 0; i < src.size().width; i++)
        {
/*check whether or not we have to move the block pointer*/
            if ((i % 32 == 0) && !((j == 0) && (i == 0)))
            {
                blockPointer += blockDelta;
            }

/* Get value from cv::Mat*/
            const unsigned char cell = src.at < unsigned char > (j, i);
            bool isBlack = false;
            if ((cell != 0) && (cell != 0xff))
            {}
            else if (cell == 0)
            {
                isBlack = true;
            }
            if (isBlack)
            {
/*define value to set*/
                unsigned char value = 0;
                int r = i % 4;
                switch (r)
                {
                    case 0:
                        value += 8;
                        break;
                    case 1:
                        value += 4;
                        break;
                    case 2:
                        value += 2;
                        break;
                    default:
                        value += 1;
                        break;
                }

/*define increasable position*/
                unsigned char* incPos = blockPointer;

                int ii = i % 32;
                switch ((int) (ii / 8))
                {
                    case 1:
                        incPos -= 1;
                        break;
                    case 2:
                        incPos -= 2;
                        break;
                    case 3:
                        incPos -= 3;
                        break;
                    default:
                        break;
                }
/*define increment value*/

                int vv = i % 8;
                if (vv < 4)
                {
                    *incPos += value * 16;
                }
                else
                {
                    *incPos += value;
                }
            }
        }
    }

    return pix;
}

cv::Mat prl::ImgLeptonToOpenCV(const PIX* src)
{
    cv::Size size(src->w, src->h);
    cv::Mat mat = cv::Mat(size, CV_8UC1);
    const unsigned char* pp = (unsigned char*) (src->data);
    const int psize = hmod(src->w, 32) * 4 * src->h;

    l_uint32 i = 0;
    int j = 0;

    int k = 3;

    while (k < psize)
    {
        unsigned char bValue = pp[k];
        unsigned char bHigh = bValue & 0xF0;
        unsigned char bLow = bValue & 0x0F;

        unsigned char divider = 8 * 16;

        while ((i < src->w) && (divider >= 16))
        {
            unsigned char v = bHigh / divider;
            if (v > 0)
            {
                mat.at < unsigned char > (j, i) = 0x00;
            }
            else
            {
                mat.at < unsigned char > (j, i) = 0xff;
            }
            i++;
            bHigh = bHigh - v * divider;
            divider = divider / 2;
        }

        divider = 8;

        while ((i < src->w) && (divider > 0))
        {
            unsigned char v = bLow / divider;
            if (v > 0)
            {
                mat.at < unsigned char > (j, i) = 0x00;
            }
            else
            {
                mat.at < unsigned char > (j, i) = 0xff;
            }
            i++;
            bLow = bLow - v * divider;
            divider = divider / 2;
        }

        if (i >= src->w)
        {
            i = 0;
            j++;

            while (k % 4 != 0)
            {
                k--;
            }
            k += 7;
            continue;
        }

        if (k % 4 == 0)
        {
            k += 7;
        }
        else
        {
            k--;
        }
    }

    return mat;
}
