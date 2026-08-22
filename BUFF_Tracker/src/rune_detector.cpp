#include "rune_detector.hpp"
#include <cmath>
#include <iostream>
#include <map>

using namespace rm_vision;
using namespace std;
using namespace cv;

namespace {

const bool kShowDoughnutMask = false;

Rect buildSearchRect(const Size& image_size,
                     const Point2f& center,
                     float radius,
                     float scale,
                     int min_half_size) {
    const int half = max(min_half_size, static_cast<int>(round(radius * scale)));
    const int cx = static_cast<int>(round(center.x));
    const int cy = static_cast<int>(round(center.y));

    int x = cx - half;
    int y = cy - half;
    int w = half * 2;
    int h = half * 2;

    x = max(0, min(x, image_size.width - 1));
    y = max(0, min(y, image_size.height - 1));
    w = max(1, min(w, image_size.width - x));
    h = max(1, min(h, image_size.height - y));
    return Rect(x, y, w, h);
}

}  // namespace

// Helper: distance
float rm_vision::euclidean_distance(const Point2f& p1, const Point2f& p2) {
    return sqrt(pow(p2.x - p1.x, 2) + pow(p2.y - p1.y, 2));
}

// Helper: 2D Point rotation around another point
Point2f rotatePoint(float theta, const Point2f& point, const Point2f& center) {
    float s = sin(theta);
    float c = cos(theta);

    Point2f p = point - center;
    float xnew = p.x * c + p.y * s;
    float ynew = -p.x * s + p.y * c;

    return Point2f(xnew + center.x, ynew + center.y);
}

bool lineIntersection(const Point2f& a1,
                      const Point2f& a2,
                      const Point2f& b1,
                      const Point2f& b2,
                      Point2f& intersection) {
    const Point2f r = a2 - a1;
    const Point2f s = b2 - b1;
    const float denominator = r.x * s.y - r.y * s.x;
    if (fabs(denominator) < 1e-6f) {
        return false;
    }

    const Point2f diff = b1 - a1;
    const float t = (diff.x * s.y - diff.y * s.x) / denominator;
    intersection = Point2f(a1.x + t * r.x, a1.y + t * r.y);
    return true;
}

// BBox intersection helpers
float rm_vision::compute_IoU(const BBox& a, const BBox& b) {
    int xLeft = max(a.xmin, b.xmin);
    int yTop = max(a.ymin, b.ymin);
    int xRight = min(a.xmax, b.xmax);
    int yBottom = min(a.ymax, b.ymax);

    if (xRight < xLeft || yBottom < yTop) return 0.0f;

    int intersectionArea = (xRight - xLeft) * (yBottom - yTop);
    int aArea = a.area();
    int bArea = b.area();

    float iou = (float)intersectionArea / (float)(aArea + bArea - intersectionArea);
    return iou;
}

// Simplified CIoU for now
float rm_vision::compute_CIoU(const BBox& a, const BBox& b) {
    float iou = compute_IoU(a, b);
    
    // Center distance
    Point2f aCenter = a.center_2f();
    Point2f bCenter = b.center_2f();
    float c2 = pow(euclidean_distance(aCenter, bCenter), 2);

    // Diagonal of bounding box
    int xmin = min(a.xmin, b.xmin);
    int ymin = min(a.ymin, b.ymin);
    int xmax = max(a.xmax, b.xmax);
    int ymax = max(a.ymax, b.ymax);
    float d2 = pow(xmax - xmin, 2) + pow(ymax - ymin, 2);
    
    if(d2 == 0) return iou;

    // Aspect ratio term v
    float v = (4.0f / (CV_PI * CV_PI)) * pow(atan2(a.width(), a.height()) - atan2(b.width(), b.height()), 2);
    
    float alpha = v / (1 - iou + v + 1e-6);
    
    return iou - (c2 / d2) - alpha * v;
}

// --- RotationRectangle Translated ---
RotationRectangle::RotationRectangle(const vector<Point2f>& pts, const Point2f& r_center) {
    if (pts.size() < 4) {
        p1 = p2 = p3 = p4 = Point2f(0.0f, 0.0f);
        top = btm = Point2f(0.0f, 0.0f);
        disTop = disBtm = 0.0f;
        points.assign(4, Point2f(0.0f, 0.0f));
        return;
    }
    
    // Basic sorting by distance from r_center
    std::vector<std::pair<cv::Point2f, float>> p;
    for (const auto& pt : pts) {
        p.push_back({pt, euclidean_distance(pt, r_center)});
    }
    std::sort(p.begin(), p.end(), [](const auto& a, const auto& b) {
        return a.second > b.second;
    });

    p1 = p[0].first;
    p2 = p[1].first;
    
    if (euclidean_distance(p1, p[2].first) > euclidean_distance(p1, p[3].first)) {
        p3 = p[2].first;
        p4 = p[3].first;
    } else {
        p3 = p[3].first;
        p4 = p[2].first;
    }

    // Simplification for top and btm (line center of p1-p2 and p3-p4)
    top = Point2f((p1.x + p2.x) / 2.0f, (p1.y + p2.y) / 2.0f);
    btm = Point2f((p3.x + p4.x) / 2.0f, (p3.y + p4.y) / 2.0f);
    disTop = euclidean_distance(top, r_center);
    disBtm = euclidean_distance(btm, r_center);

    points = {p1, p2, p3, p4};
}

Point2f RotationRectangle::center_2f() const {
    Point2f center;
    if (lineIntersection(p1, p3, p2, p4, center)) {
        return center;
    }
    return Point2f((p1.x + p2.x + p3.x + p4.x) * 0.25f,
                   (p1.y + p2.y + p3.y + p4.y) * 0.25f);
}

float RotationRectangle::width() const {
    return euclidean_distance(p1, p2);
}

float RotationRectangle::height() const {
    return euclidean_distance(p1, p4);
}

float RotationRectangle::area() const {
    return width() * height();
}


// --- RuneDetector ---
RuneDetector::RuneDetector() {
    fanNum = 0;
    states = {"target", "unlighted", "unlighted", "unlighted", "unlighted"};
    for (int i = 0; i < 5; ++i) {
        FanBladeList.push_back(FanBlade(BBox(0, 0, 0, 0, i), RotationRectangle()));
    }
}

bool RuneDetector::init(const Rect& r_rect, const Rect& fan_rect) {
    R_Box = BBox(r_rect.x, r_rect.y, r_rect.x + r_rect.width, r_rect.y + r_rect.height);
    fanBladeBox = BBox(fan_rect.x, fan_rect.y, fan_rect.x + fan_rect.width, fan_rect.y + fan_rect.height);
    radius = euclidean_distance(R_Box.center_2f(), fanBladeBox.center_2f());
    return true;
}

Mat RuneDetector::getMaskByHSVThreshold(const Mat& frame) {
    Mat hsv, mask;
    cvtColor(frame, hsv, COLOR_BGR2HSV);
    inRange(hsv, param.lowerLimit, param.upperLimit, mask);

    Mat kernel = Mat::ones(Size(param.kernel_size, param.kernel_size), CV_8U);
    dilate(mask, mask, kernel, Point(-1, -1), 1);
    
    return mask;
}

bool RuneDetector::mayBeTarget(int w, int h, bool flag) {
    if (!flag) return true;
    int r_area = R_Box.area();
    int area = w * h;
    if (r_area == 0) return true;
    float area_ratio = (float)min(area, r_area) / (float)max(area, r_area);
    float w_ratio = (float)min(w, (int)R_Box.width()) / (float)max(w, (int)R_Box.width());
    float h_ratio = (float)min(h, (int)R_Box.height()) / (float)max(h, (int)R_Box.height());
    
    return area_ratio > param.mb_area && w_ratio > param.mb_width && h_ratio > param.mb_height;
}

std::vector<BBox> RuneDetector::getAlternateBoxs(const cv::Mat& mask, bool isOpenMaybeTarget) {
    vector<vector<Point>> contours;
    float rad = euclidean_distance(R_Box.center_2f(), fanBladeBox.center_2f());
    radius = (radius == 0) ? rad : radius; // prevent divide by zero

    const Rect search_rect = buildSearchRect(mask.size(), R_Box.center_2f(), max(radius, 1.0f), 2.8f, 80);
    Mat search_mask = mask(search_rect);
    findContours(search_mask, contours, RETR_EXTERNAL, CHAIN_APPROX_SIMPLE);

    vector<BBox> boxs;
    
    for (const auto& cont : contours) {
        Rect rect = boundingRect(cont);
        rect.x += search_rect.x;
        rect.y += search_rect.y;
        float dis = euclidean_distance(Point2f(rect.x + rect.width / 2.0f, rect.y + rect.height / 2.0f), R_Box.center_2f());
        
        if (mayBeTarget(rect.width, rect.height, isOpenMaybeTarget) && dis < radius * 2.0f) {
            boxs.push_back(BBox(rect.x, rect.y, rect.x + rect.width, rect.y + rect.height));
        }
    }
    return boxs;
}

std::vector<FanBlade> RuneDetector::getFanBlade(cv::Mat& mask) {
    vector<vector<Point>> contours;
    vector<FanBlade> fanBladeList;
    vector<FanBlade> realFanBladeList;
    
    if (R_Box.area() == 0 || radius <= 0) return vector<FanBlade>();

    const float blade_scale = max(1.8f, param.outsideRate + 0.6f);
    const Rect search_rect = buildSearchRect(mask.size(), R_Box.center_2f(), max(radius, 1.0f), blade_scale, 80);
    Mat search_mask = mask(search_rect);
    findContours(search_mask, contours, RETR_EXTERNAL, CHAIN_APPROX_SIMPLE);

    for (const auto& cont : contours) {
        RotatedRect rect = minAreaRect(cont);
        Point2f rect_pts[4];
        rect.points(rect_pts);
        vector<Point2f> pts_vec;
        pts_vec.reserve(4);
        for (int i = 0; i < 4; ++i) {
            pts_vec.push_back(Point2f(rect_pts[i].x + search_rect.x, rect_pts[i].y + search_rect.y));
        }
        
        RotationRectangle rtn_rect(pts_vec, R_Box.center_2f());
        
        const bool enable_blade_console_log = false;
        if (enable_blade_console_log && rtn_rect.area() > 0.5f * R_Box.area()) {
            cout << "[DEBUG Blade] disBtm: " << rtn_rect.disBtm 
                 << " | disTop: " << rtn_rect.disTop 
                 << " | area: " << rtn_rect.area() 
                 << " || Thresholds -> radius: " << radius 
                 << " | R_area: " << R_Box.area() << endl;
        }

        // Strict proportion checks matching Python logic: 0.4 * radius < disBtm < disTop < 1.5 * radius AND area > 2 * R_area
        if (rtn_rect.disBtm > 0.4f * radius && rtn_rect.disBtm < rtn_rect.disTop && rtn_rect.disTop < 1.5f * radius && rtn_rect.area() > 2.0f * R_Box.area()) {
            // Match Python original: use boundingRect(cont) for axis-aligned bbox
            Rect br = boundingRect(cont);
            br.x += search_rect.x;
            br.y += search_rect.y;
            BBox box(br.x, br.y, br.x + br.width, br.y + br.height);
            fanBladeList.push_back(FanBlade(box, rtn_rect));
        }
    }
    
    if (fanBladeList.empty()) {
        return vector<FanBlade>();
    }
    if (FanBladeList.empty() || FanBladeList[0].bbox.area() == 0) {
        if (fanBladeList.size() == 1) {
            fanBladeList[0].bbox.id = 0;
            return fanBladeList;
        } else {
            return vector<FanBlade>(); // Return empty until we lock onto 1
        }
    }
    
    vector<FanBlade> correctFanBlade;
    for (size_t i = 0; i < FanBladeList.size(); ++i) {
        Point2f offset = R_Box.center_2f() - center;
        Point2f new_center = FanBladeList[i].bbox.center_2f() + offset;
        BBox shifted = FanBladeList[i].bbox.create_new_bbox_by_center(new_center);
        shifted.id = FanBladeList[i].bbox.id;
        
        vector<Point2f> tempPoints;
        for (const auto& p : FanBladeList[i].rtn_rect.points) {
            tempPoints.push_back(p + offset);
        }
        
        correctFanBlade.push_back(FanBlade(shifted, RotationRectangle(tempPoints, R_Box.center_2f())));
    }
    
    std::map<int, vector<FanBlade>> tempList;
    for (const auto& lastFan : correctFanBlade) {
        tempList[lastFan.bbox.id] = {};  // Python original: tempList[box.id] = []
        for (const auto& fan : fanBladeList) {
            // IoU > 0: only match if boxes actually overlap (Python original)
            if (compute_IoU(lastFan.bbox, fan.bbox) > 0) {
                tempList[lastFan.bbox.id].push_back(fan);
            }
        }
        
        if (tempList[lastFan.bbox.id].size() == 1) {
            tempList[lastFan.bbox.id][0].bbox.id = lastFan.bbox.id;
            realFanBladeList.push_back(tempList[lastFan.bbox.id][0]);
        } else if (tempList[lastFan.bbox.id].size() >= 2) {
            // Full merge logic for broken/shot fan blades matching Python
            vector<pair<Point2f, float>> tempP1, tempP2, tempP3, tempP4;
            for (const auto& rtn : tempList[lastFan.bbox.id]) {
                tempP1.push_back({rtn.rtn_rect.p1, euclidean_distance(rtn.rtn_rect.p1, R_Box.center_2f())});
                tempP2.push_back({rtn.rtn_rect.p2, euclidean_distance(rtn.rtn_rect.p2, R_Box.center_2f())});
                tempP3.push_back({rtn.rtn_rect.p3, euclidean_distance(rtn.rtn_rect.p3, R_Box.center_2f())});
                tempP4.push_back({rtn.rtn_rect.p4, euclidean_distance(rtn.rtn_rect.p4, R_Box.center_2f())});
            }
            
            auto p1_pair = *max_element(tempP1.begin(), tempP1.end(), [](const auto& a, const auto& b) { return a.second < b.second; });
            auto p2_pair = *max_element(tempP2.begin(), tempP2.end(), [](const auto& a, const auto& b) { return a.second < b.second; });
            auto p3_pair = *min_element(tempP3.begin(), tempP3.end(), [](const auto& a, const auto& b) { return a.second < b.second; });
            auto p4_pair = *min_element(tempP4.begin(), tempP4.end(), [](const auto& a, const auto& b) { return a.second < b.second; });
            
            vector<Point2f> merged_pts = {p1_pair.first, p2_pair.first, p3_pair.first, p4_pair.first};
            
            float xmin = min({merged_pts[0].x, merged_pts[1].x, merged_pts[2].x, merged_pts[3].x});
            float ymin = min({merged_pts[0].y, merged_pts[1].y, merged_pts[2].y, merged_pts[3].y});
            float xmax = max({merged_pts[0].x, merged_pts[1].x, merged_pts[2].x, merged_pts[3].x});
            float ymax = max({merged_pts[0].y, merged_pts[1].y, merged_pts[2].y, merged_pts[3].y});
            
            BBox merged_bbox(xmin, ymin, xmax, ymax);
            merged_bbox.id = lastFan.bbox.id;
            realFanBladeList.push_back(FanBlade(merged_bbox, RotationRectangle(merged_pts, R_Box.center_2f())));
        }
    }
    
    // State machine logic matching Python exactly
    for (size_t i = 0; i < realFanBladeList.size(); ++i) {
        if (realFanBladeList.size() == 1) {
            realFanBladeList[0].bbox.id = 0;
            states[0] = "target";
            for (int i_ = 1; i_ < 5; ++i_) {
                states[i_] = "unlighted";
            }
        } else if (realFanBladeList.size() > fanNum) {
            int id_x = realFanBladeList[i].bbox.id;
            if (id_x >= 0 && id_x < 5) {
                if (states[id_x] == "target") {
                    states[id_x] = "shot";
                } else if (states[id_x] == "unlighted") {
                    states[id_x] = "target";
                }
            }
        }
    }
    
    return realFanBladeList;
}

bool RuneDetector::update(Mat& frame, bool isOpenMaybeTarget) {
    debug_frame = frame.clone();
    
    Mat mask = getMaskByHSVThreshold(frame);
    
    // 1. Get alternate boxes and find R
    vector<BBox> boxs = getAlternateBoxs(mask, isOpenMaybeTarget);
    float max_ciou = -10.0f;
    BBox best_r = R_Box;
    for (const auto& b : boxs) {
        float iou = compute_CIoU(R_Box, b);
        if (iou > max_ciou) {
            max_ciou = iou;
            best_r = b;
        }
    }
    
    if (max_ciou > -1.0f && best_r.area() > 0) {
        R_Box = best_r;
    } else {
        return false;
    }
    
    // Debug R
    rectangle(debug_frame, Point(R_Box.xmin, R_Box.ymin), Point(R_Box.xmax, R_Box.ymax), Scalar(0, 0, 255), 3);
    putText(debug_frame, "R", Point(R_Box.xmin, R_Box.ymin), FONT_HERSHEY_SIMPLEX, 0.75, Scalar(0, 0, 255), 2);
    
    // 2. Draw Doughnut to occlude R and noise
    circle(mask, R_Box.center_2i(), int(radius * param.insideRate), Scalar(0), -1);
    circle(mask, R_Box.center_2i(), int(radius * param.outsideRate), Scalar(0), 3);
    
    // Show extracted mask for blades
    if (kShowDoughnutMask) {
        imshow("Doughnut_Mask", mask);
    }
    
    // 3. Get Blades
    auto fanBladeList = getFanBlade(mask);
    
    center = R_Box.center_2f();
    
    if (fanBladeList.empty()) return false;
    
    fanNum = fanBladeList.size();
    
    for (const auto& fan : fanBladeList) {
        BBox box = fan.bbox;
        
        string state_str = "unknown";
        if (box.id >= 0 && box.id < 5) {
            FanBladeList[box.id] = fan; // Assign full structural data (points, bounding boxes, etc)
            state_str = states[box.id];
            
            if (state_str == "target") {
                fanBladeBox = box;
            }
        }
        
        rectangle(debug_frame, Point(box.xmin, box.ymin), Point(box.xmax, box.ymax), Scalar(255, 0, 0), 2);
        putText(debug_frame, "id: " + to_string(box.id) + " | " + state_str, 
                Point(box.xmin, box.ymin), FONT_HERSHEY_SIMPLEX, 0.75, Scalar(255, 0, 0), 2);
    }
    
    // Update unlighted fan blade info based on Target FanBlade using Polar Math
    for (int i = 0; i < 5; ++i) {
        // Skip blades that are already tracking something lit/shot
        bool isLit = false;
        for (const auto& fan : fanBladeList) {
            if (fan.bbox.id == i) isLit = true;
        }
        if (isLit) continue;
        
        // Ensure fanBladeBox (Target) is populated
        if (fanBladeBox.area() > 0 && FanBladeList[0].bbox.area() > 0) {
            Point2f rotated_center = rotatePoint(CV_PI * 2.0f / 5.0f * i, FanBladeList[0].bbox.center_2f(), R_Box.center_2f());
            FanBladeList[i].bbox = fanBladeBox.create_new_bbox_by_center(rotated_center);
            FanBladeList[i].bbox.id = i;
        }
    }
    
    return true;
} // namespace rm_vision
