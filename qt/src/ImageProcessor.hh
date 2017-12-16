#ifndef GIMAGEREADER_IMAGEPROCESSING_HH
#define GIMAGEREADER_IMAGEPROCESSING_HH

#include <QObject>
#include <QImage>

#include <opencv2/core/core.hpp>

class Displayer;
class UI_MainWindow;

enum MatColorOrder {
    MCO_BGR,
    MCO_RGB,
    MCO_BGRA = MCO_BGR,
    MCO_RGBA = MCO_RGB,
    MCO_ARGB
};

class ImageProcessor : public QObject {
    Q_OBJECT
public:
    ImageProcessor(const UI_MainWindow& _ui, Displayer& _displayer);
    ~ImageProcessor();

    signals:

private:
    cv::Mat imageToMat(const QImage& img, int requiredMatType = CV_8UC(0), MatColorOrder requiredOrder = MCO_BGR);
    QImage matToImage(const cv::Mat& mat, MatColorOrder order = MCO_BGR,
                      QImage::Format formatHint = QImage::Format_Invalid);
    cv::Mat imageToMat_shared(const QImage& img, MatColorOrder* order = nullptr);
    QImage matToImage_shared(const cv::Mat& mat, QImage::Format formatHint = QImage::Format_Invalid);

private:
    const UI_MainWindow& ui;
    Displayer& displayer;

private slots:
    void deskew();
};

#endif //GIMAGEREADER_IMAGEPROCESSING_HH
