#include "angle_predictor.hpp"

#include <algorithm>
#include <cmath>
#include <limits>
#include <numeric>
#include <stdexcept>

using namespace rm_vision;
using namespace cv;

namespace {

double pointDistance(const Point2f& p1, const Point2f& p2) {
    const double dx = static_cast<double>(p2.x - p1.x);
    const double dy = static_cast<double>(p2.y - p1.y);
    return std::sqrt(dx * dx + dy * dy);
}

}  // namespace

CircularQueue::CircularQueue(int queue_capacity)
    : capacity_(std::max(1, queue_capacity)),
      queue_(capacity_, -1.0),
      front_index_(0),
      rear_index_(0) {}

void CircularQueue::push(double data) {
    if (isFull()) {
        throw std::runtime_error("queue is full");
    }
    queue_[front_index_ % capacity_] = data;
    ++front_index_;
}

void CircularQueue::pop() {
    if (isEmpty()) {
        throw std::runtime_error("queue is empty");
    }
    ++rear_index_;
}

bool CircularQueue::isFull() const {
    return size() == capacity_;
}

bool CircularQueue::isEmpty() const {
    return size() == 0;
}

int CircularQueue::size() const {
    return front_index_ - rear_index_;
}

double CircularQueue::front() const {
    if (isEmpty()) {
        throw std::runtime_error("queue is empty");
    }
    return queue_[(front_index_ - 1) % capacity_];
}

double CircularQueue::rear() const {
    if (isEmpty()) {
        throw std::runtime_error("queue is empty");
    }
    return queue_[rear_index_ % capacity_];
}

MovAvg::MovAvg(int window_size)
    : window_size_(std::max(1, window_size)),
      pre_sum_(window_size_),
      data_queue_(window_size_),
      first_time_(true) {}

double MovAvg::update(double data) {
    if (first_time_) {
        pre_sum_.push(data);
        first_time_ = false;
    } else {
        if (pre_sum_.isFull()) {
            pre_sum_.pop();
            pre_sum_.push(pre_sum_.front() - data_queue_.rear() + data);
            data_queue_.pop();
        } else {
            pre_sum_.push(pre_sum_.front() + data);
        }
    }

    data_queue_.push(data);
    return pre_sum_.front() / static_cast<double>(pre_sum_.size());
}

FitStartDetect::FitStartDetect(int queue_capacity)
    : queue_capacity_(std::max(1, queue_capacity)),
      queue_(queue_capacity_),
      idx_(0),
      flip_count_(0),
      lim_(20),
      count_(0) {}

bool FitStartDetect::isFlip() const {
    if (idx_ <= 1 || derivative_.size() < 2) {
        return false;
    }
    return derivative_[idx_ - 2] * derivative_[idx_ - 1] < 0.0;
}

std::pair<bool, int> FitStartDetect::update(double data) {
    queue_.push(data);
    if (queue_.isFull()) {
        queue_.pop();
        derivative_.push_back((queue_.front() - queue_.rear()) / static_cast<double>(queue_capacity_));
        ++idx_;

        if (isFlip()) {
            ++flip_count_;
        }

        if (flip_count_ == 2) {
            if (count_ < lim_) {
                ++count_;
            } else {
                return {true, idx_};
            }
        }

        if (flip_count_ > 2) {
            throw std::runtime_error("fit start detect unstable, adjust smoothing or queue size");
        }
    }

    return {false, -1};
}

BigPredictor::BigPredictor(double delta_t, double freq)
    : frame_interval_(std::max(1, static_cast<int>(std::ceil(freq * delta_t)))),
      start_fit_(),
      smooth_(20),
      slid_win_(frame_interval_),
      is_start_(false),
      has_params_(false),
      x_(0),
      a0_(0.0),
      a1_(0.0),
      a2_(0.0),
      a3_(0.0) {}

double BigPredictor::targetFunc(double x, double a0, double a1, double a2, double a3) {
    return a0 * std::sin(a1 * x + a2) + a3;
}

double BigPredictor::targetFunc(double x) const {
    return targetFunc(x, a0_, a1_, a2_, a3_);
}

bool BigPredictor::fitSinParams() {
    const int n = static_cast<int>(diff_y_.size());
    if (n < 8) {
        return false;
    }

    Mat signal(n, 1, CV_64F);
    for (int i = 0; i < n; ++i) {
        signal.at<double>(i, 0) = diff_y_[i];
    }

    Mat spectrum;
    dft(signal, spectrum, DFT_COMPLEX_OUTPUT);

    const int half_n = std::max(2, n / 2);
    int best_idx = 1;
    double best_mag = 0.0;
    for (int k = 1; k < half_n; ++k) {
        const Vec2d val = spectrum.at<Vec2d>(k, 0);
        const double mag = std::sqrt(val[0] * val[0] + val[1] * val[1]);
        if (mag > best_mag) {
            best_mag = mag;
            best_idx = k;
        }
    }

    double base_w = 2.0 * CV_PI * (static_cast<double>(best_idx) / static_cast<double>(n));
    if (base_w < 1e-6) {
        base_w = 2.0 * CV_PI / static_cast<double>(n);
    }

    double best_w = base_w;
    Mat best_coeff;
    double best_err = std::numeric_limits<double>::max();

    for (int step = -10; step <= 10; ++step) {
        const double ratio = 1.0 + 0.03 * static_cast<double>(step);
        const double w = std::max(1e-6, base_w * ratio);

        Mat a(n, 3, CV_64F);
        Mat b(n, 1, CV_64F);

        for (int i = 0; i < n; ++i) {
            const double x = static_cast<double>(i);
            a.at<double>(i, 0) = std::sin(w * x);
            a.at<double>(i, 1) = std::cos(w * x);
            a.at<double>(i, 2) = 1.0;
            b.at<double>(i, 0) = diff_y_[i];
        }

        Mat coeff;
        if (!solve(a, b, coeff, DECOMP_SVD)) {
            continue;
        }

        Mat residual = a * coeff - b;
        const double err = norm(residual, NORM_L2SQR) / static_cast<double>(n);
        if (err < best_err) {
            best_err = err;
            best_w = w;
            best_coeff = coeff;
        }
    }

    if (best_coeff.empty()) {
        return false;
    }

    const double b_sin = best_coeff.at<double>(0, 0);
    const double b_cos = best_coeff.at<double>(1, 0);
    const double bias = best_coeff.at<double>(2, 0);

    a0_ = std::sqrt(b_sin * b_sin + b_cos * b_cos);
    a1_ = best_w;
    a2_ = std::atan2(b_cos, b_sin);
    a3_ = bias;
    has_params_ = true;
    return true;
}

std::pair<bool, double> BigPredictor::update(double angle) {
    y_.push_back(angle);
    ++x_;
    slid_win_.push(angle);

    if (slid_win_.isFull()) {
        double diff = slid_win_.front() - slid_win_.rear();
        diff = smooth_.update(diff);
        diff_y_.push_back(diff);
        slid_win_.pop();

        if (!is_start_) {
            const auto fit_flag = start_fit_.update(diff);
            is_start_ = fit_flag.first;
        }

        if (is_start_) {
            if (!has_params_) {
                if (!fitSinParams()) {
                    return {false, 0.0};
                }
            }
            const double delta_y = targetFunc(static_cast<double>(x_ + frame_interval_));
            return {true, delta_y};
        }
    }

    return {false, 0.0};
}

SmallPredictor::SmallPredictor(double delta_t, double freq)
    : win_size_(std::max(1, static_cast<int>(std::ceil(freq * delta_t)))), pred_(0.0) {}

std::pair<bool, double> SmallPredictor::update(double angle) {
    y_.push_back(angle);

    if (static_cast<int>(y_.size()) == win_size_) {
        if (win_size_ < 2) {
            pred_ = 0.0;
        } else {
            double sum = 0.0;
            for (int i = 1; i < win_size_; ++i) {
                sum += (y_[i] - y_[i - 1]);
            }
            pred_ = sum / static_cast<double>(win_size_ - 1);
        }
        return {true, pred_ * static_cast<double>(win_size_)};
    }

    if (static_cast<int>(y_.size()) > win_size_) {
        return {true, pred_ * static_cast<double>(win_size_)};
    }

    return {false, 0.0};
}

double rm_vision::trans_angle(double x, double y) {
    double angle = std::atan(y / x);
    if (x > 0 && y > 0) {
        return angle;
    }
    if (x < 0) {
        angle += CV_PI;
    } else {
        angle += 2.0 * CV_PI;
    }
    return angle;
}

AngleObserver::AngleObserver(ClockMode clock_mode)
    : last_y_(-1.0),
      last_x_(-1.0),
      delta_(0),
      clock_mode_(clock_mode) {}

Point2f AngleObserver::rotation(double theta, const Point2f& vector) {
    Matx22f rotation_matrix(std::cos(theta), std::sin(theta),
                            -std::sin(theta), std::cos(theta));
    const Vec2f input(vector.x, vector.y);
    const Vec2f output = rotation_matrix * input;
    return Point2f(output[0], output[1]);
}

double AngleObserver::angleTransformer(double x, double y) {
    double theta = std::atan(y / x);
    if (last_angle_.empty()) {
        last_angle_.push_back(theta);
        return theta;
    }

    double delta = std::fabs(std::round((theta - last_angle_[0]) / CV_PI)) * CV_PI;
    if (clock_mode_ == ClockMode::anticlockwise) {
        delta *= -1.0;
    }
    theta += delta;
    last_angle_[0] = theta;
    return theta;
}

double AngleObserver::update(double x, double y, double radius) {
    if (last_x_ == -1.0 && last_y_ == -1.0) {
        last_x_ = x;
        last_y_ = y;
    }

    if (delta_ != 0) {
        const Point2f rotated = rotation(2.0f * CV_PI / 5.0f * (5 - delta_), Point2f(x, y));
        x = rotated.x;
        y = rotated.y;
    }

    if (pointDistance(Point2f(static_cast<float>(last_x_), static_cast<float>(last_y_)),
                      Point2f(static_cast<float>(x), static_cast<float>(y))) > radius * 0.5) {
        std::vector<Point2f> points;
        points.reserve(5);
        for (int t = 0; t < 5; ++t) {
            points.push_back(rotation(2.0f * CV_PI / 5.0f * t,
                                      Point2f(static_cast<float>(last_x_), static_cast<float>(last_y_))));
        }

        int min_idx = 0;
        double min_dist = std::numeric_limits<double>::max();
        for (int i = 0; i < 5; ++i) {
            const double dist = pointDistance(Point2f(static_cast<float>(x), static_cast<float>(y)), points[i]);
            if (dist < min_dist) {
                min_dist = dist;
                min_idx = i;
            }
        }

        const Point2f rotated = rotation(2.0f * CV_PI / 5.0f * (5 - min_idx), Point2f(x, y));
        x = rotated.x;
        y = rotated.y;
        delta_ += min_idx;
    }

    const double angle = angleTransformer(x, y);
    last_x_ = x;
    last_y_ = y;
    return angle;
}
