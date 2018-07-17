//
// Z3D - A structured light 3D scanner
// Copyright (C) 2013-2016 Nicolas Ulrich <nikolaseu@gmail.com>
//
// This file is part of Z3D.
//
// Z3D is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// Z3D is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with Z3D.  If not, see <http://www.gnu.org/licenses/>.
//

#include "zbinarypatterndecoder.h"

#include "zdecodedpattern.h"

#include <map>

#include <opencv2/imgproc.hpp>

#include <QDebug>

namespace Z3D
{

namespace ZBinaryPatternDecoder
{

template<typename T>
T binaryToGray(T num)
{
    return (num >> 1) ^ num;
}


template<typename T>
T grayToBinary(T num)
{
    const unsigned long len = 8 * sizeof(num);

    /// mask the MSB.
    unsigned int grayBit = 1 << ( len - 1 );
    /// copy the MSB.
    T binary = num & grayBit;
    /// store the bit we just set.
    unsigned int binBit = binary;
    /// traverse remaining Gray bits.
    while( grayBit >>= 1 ) {
        /// shift the current binary bit
        /// to align with the Gray bit.
        binBit >>= 1;
        /// XOR the two bits.
        binary |= binBit ^ ( num & grayBit );
        /// store the current binary bit
        binBit = binary & grayBit;
    }

    return binary;
}


cv::Mat decodeBinaryPatternImages(const std::vector<cv::Mat> &images, const std::vector<cv::Mat> &invImages, const cv::Mat &maskImg, const bool isGrayCode)
{
    const size_t imgCount = images.size();

    const cv::Size &imgSize = images[0].size();

    /// use 16 bits, it's more than enough
    cv::Mat decodedImg(imgSize, CV_16U, cv::Scalar(Z3D::ZDecodedPattern::NO_VALUE));

    const int &imgHeight = imgSize.height;
    const int &imgWidth = imgSize.width;

    for (unsigned int i=0; i<imgCount; ++i) {
        const cv::Mat &regImg = images[i];
        const cv::Mat &invImg = invImages[i];
        const uint16_t bit = uint16_t(1 << ( i+1 ));
        for (int y=0; y<imgHeight; ++y) {
            /// get pointers to first item of the row
            const uint8_t* maskImgData = maskImg.ptr<uint8_t>(y);
            const uint8_t* imgData = regImg.ptr<uint8_t>(y);
            const uint8_t* invImgData = invImg.ptr<uint8_t>(y);
            uint16_t* decodedImgData = decodedImg.ptr<uint16_t>(y);
            for (int x=0; x<imgWidth; ++x) {
                if (*maskImgData) {
                    uint16_t &value = *decodedImgData;
                    if (*imgData > *invImgData) {
                        /// enable bit
                        value |= bit;
                    }
                }

                /// don't forget to advance pointers!!
                maskImgData++;
                imgData++;
                invImgData++;
                decodedImgData++;
            }
        }
    }

    /// convert gray code to binary, i.e. "normal" value
    if (isGrayCode) {
        for (int y=0; y<imgHeight; ++y) {
            /// get pointers to first item of the row
            uint16_t* decodedImgData = decodedImg.ptr<uint16_t>(y);
            for (int x=0; x<imgWidth; ++x, ++decodedImgData) {
                uint16_t &value = *decodedImgData;
                if (value != Z3D::ZDecodedPattern::NO_VALUE) {
                    value = grayToBinary(value);
                }
            }
        }
    }

    /// fill single pixel holes
    for (int y=0; y<imgHeight-1; ++y) {
        uint16_t *decodedImgData = decodedImg.ptr<uint16_t>(y) + 1; // start in x+1
        for (int x=1; x<imgWidth-1; ++x, ++decodedImgData) {
            uint16_t &value = *decodedImgData;
            if (value != Z3D::ZDecodedPattern::NO_VALUE) {
                continue;
            }

            const uint16_t valueLeft = *(decodedImgData - 1);
            const uint16_t valueRight = *(decodedImgData + 1);
            if (valueLeft != Z3D::ZDecodedPattern::NO_VALUE && valueRight == valueLeft) {
                /// assign value
                value = valueLeft;
            }
        }
    }

    return decodedImg;
}


cv::Mat simplifyBinaryPatternData(cv::Mat image, cv::Mat maskImg, std::map<int, std::vector<cv::Vec2f> > &fringePoints)
{
    const cv::Size &imgSize = image.size();

    /// use 16 bits, it's enough
    cv::Mat decodedImg(imgSize, CV_16U, cv::Scalar(0));

    const int &imgHeight = imgSize.height;
    const int &imgWidth = imgSize.width;

    for (int y=0; y<imgHeight; ++y) {
        /// get pointers to first item of the row
        const uint8_t* maskImgData = maskImg.ptr<uint8_t>(y);
        const uint16_t* imgData = image.ptr<uint16_t>(y);
        uint16_t* decodedImgData = decodedImg.ptr<uint16_t>(y);
        for (int x=0; x<imgWidth-1; ++x) {
            /// get mask pixels
            const uint8_t &maskValue     = *maskImgData;
            maskImgData++; /// always advance pointer
            const uint8_t &nextMaskValue = *maskImgData;

            /// get image pixels
            const uint16_t &value     = *imgData;
            imgData++; /// always advance pointer
            const uint16_t &nextValue = *imgData;

            if (maskValue && nextMaskValue && nextValue != value) {
                *decodedImgData = value;
                //decodedImgData[x+1] = nextValue; // not both borders, it's useless. x => x+0.5
                uint16_t imin = std::min(value, nextValue),
                               imax = std::max(value, nextValue);
                cv::Vec2f point(0.5f+x, y); // x => x+0.5
                if (false) { /// THIS SHOULD BE ALWAYS FALSE
                    /// todos?
                    for (int i = imin; i < imax; ++i) { // not "<=" because we use the left border only
                        fringePoints[i].push_back(point);
                    }
                } else {
                    /// solo "bordes"
                    fringePoints[imin].push_back(point);
                    if (imax - imin > 1)
                        fringePoints[imax-1].push_back(point);
                }
            }

            /// always advance pointer
            decodedImgData++;
        }
    }

    return decodedImg;
}

} // namespace ZBinaryPatternDecoder

} // namespace Z3D
