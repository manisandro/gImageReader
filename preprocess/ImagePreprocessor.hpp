//
// Created by zamazan4ik on 14.06.17.
//

#ifndef GIMAGEREADER_IMAGEPREPROCESSOR_HPP
#define GIMAGEREADER_IMAGEPREPROCESSOR_HPP

#include <QImage>

namespace prl
{
class ImagePreprocessor
{
public:
    static QImage preprocess(const QImage& image);
};
}

#endif //GIMAGEREADER_IMAGEPREPROCESSOR_HPP
