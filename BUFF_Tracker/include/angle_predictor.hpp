#ifndef ANGLE_PREDICTOR_HPP
#define ANGLE_PREDICTOR_HPP

#include <opencv2/core.hpp>
#include <utility>
#include <vector>

namespace rm_vision {

enum class MoveMode {
    big,
    small
};

enum class ClockMode {
    anticlockwise,
    clockwise
};

class CircularQueue {
public:
    explicit CircularQueue(int queue_capacity);

    void push(double data);
    void pop();

    bool isFull() const;
    bool isEmpty() const;
    int size() const;

    double front() const;
    double rear() const;

private:
    int capacity_;
    std::vector<double> queue_;
    int front_index_;
    int rear_index_;
};

class MovAvg {
public:
    explicit MovAvg(int window_size = 7);
    double update(double data);

private:
    int window_size_;
    CircularQueue pre_sum_;
    CircularQueue data_queue_;
    bool first_time_;
};

class FitStartDetect {
public:
    explicit FitStartDetect(int queue_capacity = 15);
    std::pair<bool, int> update(double data);

private:
    bool isFlip() const;

    int queue_capacity_;
    CircularQueue queue_;
    std::vector<double> derivative_;
    int idx_;
    int flip_count_;
    int lim_;
    int count_;
};

class BigPredictor {
public:
    BigPredictor(double delta_t, double freq);
    std::pair<bool, double> update(double angle);

private:
    static double targetFunc(double x, double a0, double a1, double a2, double a3);
    double targetFunc(double x) const;
    bool fitSinParams();

    int frame_interval_;
    FitStartDetect start_fit_;
    MovAvg smooth_;
    CircularQueue slid_win_;
    bool is_start_;
    bool has_params_;
    std::vector<double> y_;
    std::vector<double> diff_y_;
    int x_;
    double a0_;
    double a1_;
    double a2_;
    double a3_;
};

class SmallPredictor {
public:
    SmallPredictor(double delta_t, double freq);
    std::pair<bool, double> update(double angle);

private:
    int win_size_;
    double pred_;
    std::vector<double> y_;
};

double trans_angle(double x, double y);

class AngleObserver {
public:
    explicit AngleObserver(ClockMode clock_mode);
    double update(double x, double y, double radius);

private:
    static cv::Point2f rotation(double theta, const cv::Point2f& vector);
    double angleTransformer(double x, double y);

    double last_y_;
    double last_x_;
    std::vector<double> last_angle_;
    int delta_;
    ClockMode clock_mode_;
};

}  // namespace rm_vision

#endif  // ANGLE_PREDICTOR_HPP
