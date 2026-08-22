#ifndef RUNE_DETECTOR_HPP
#define RUNE_DETECTOR_HPP

#include <opencv2/opencv.hpp>
#include <vector>
#include <string>

namespace rm_vision {

// Bounding Box translated from BBox
class BBox {
public:
    int id;
    int xmin, ymin, xmax, ymax;

    BBox() : id(-1), xmin(0), ymin(0), xmax(0), ymax(0) {}
    BBox(int xmin, int ymin, int xmax, int ymax, int id = -1) 
        : id(id), xmin(xmin), ymin(ymin), xmax(xmax), ymax(ymax) {}

    cv::Point2f center_2f() const {
        return cv::Point2f((xmin + xmax) / 2.0f, (ymin + ymax) / 2.0f);
    }
    
    cv::Point center_2i() const {
        return cv::Point((xmin + xmax) / 2, (ymin + ymax) / 2);
    }

    int width() const { return xmax - xmin; }
    int height() const { return ymax - ymin; }
    int area() const { return width() * height(); }

    BBox create_new_bbox_by_center(const cv::Point2f& center) const {
        int nxm = center.x - width() / 2;
        int nym = center.y - height() / 2;
        int nxma = center.x + width() / 2;
        int nyma = center.y + height() / 2;
        return BBox(nxm, nym, nxma, nyma, id);
    }
    
    // IoU and distance utilities will be declared later
};

// RotationRectangle translated from RotationRectangle
class RotationRectangle {
public:
    cv::Point2f p1, p2, p3, p4;
    cv::Point2f top, btm;
    float disTop, disBtm;
    std::vector<cv::Point2f> points;

    RotationRectangle() : points(4, cv::Point2f(0.0f, 0.0f)), disTop(0), disBtm(0) {}
    RotationRectangle(const std::vector<cv::Point2f>& pts, const cv::Point2f& r_center);
    
    cv::Point2f center_2f() const;
    float width() const;
    float height() const;
    float area() const;
};

// FanBlade translated from FanBlade
class FanBlade {
public:
    BBox bbox;
    RotationRectangle rtn_rect;
    std::string state;

    FanBlade() {}
    FanBlade(const BBox& b, const RotationRectangle& r) : bbox(b), rtn_rect(r) {}
};

// Parameters block
struct RuneParam {
    cv::Scalar lowerLimit = cv::Scalar(0, 84, 150);
    cv::Scalar upperLimit = cv::Scalar(60, 255, 255);
    int kernel_size = 7;
    float outsideRate = 1.4f;
    float insideRate = 0.73f;
    float mb_width = 0.1f;
    float mb_height = 0.1f;
    float mb_area = 0.1f;
};

class RuneDetector {
public:
    RuneParam param;
    BBox R_Box;
    BBox fanBladeBox;
    std::vector<FanBlade> FanBladeList;
    float radius;
    std::vector<std::string> states;
    int fanNum;
    cv::Point2f center;

    cv::Mat debug_frame;

    RuneDetector();
    bool init(const cv::Rect& r_rect, const cv::Rect& fan_rect);
    bool update(cv::Mat& frame, bool isOpenMaybeTarget = true);

private:
    cv::Mat getMaskByHSVThreshold(const cv::Mat& frame);
    std::vector<FanBlade> getFanBlade(cv::Mat& mask);
    std::vector<BBox> getAlternateBoxs(const cv::Mat& mask, bool isOpenMaybeTarget);
    bool mayBeTarget(int w, int h, bool flag);
};

// Math utils
float euclidean_distance(const cv::Point2f& p1, const cv::Point2f& p2);
float compute_IoU(const BBox& a, const BBox& b);
float compute_CIoU(const BBox& a, const BBox& b);

} // namespace rm_vision

#endif // RUNE_DETECTOR_HPP
