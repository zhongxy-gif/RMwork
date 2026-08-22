#include <opencv2/opencv.hpp>
#include <algorithm>
#include <cctype>
#include <cmath>
#include <iostream>
#include <string>
#include <vector>
#include "angle_predictor.hpp"
#include "rune_detector.hpp"

using namespace cv;
using namespace std;
using namespace rm_vision;

// Global variables for trackbars
int h_min = 0, s_min = 87, v_min = 150;
int h_max = 180, s_max = 255, v_max = 255;
int morph_kernel = 2;

void on_trackbar(int, void*) {}

namespace {

const string kPredictColor = "blue";
const MoveMode kPredictMoveMode = MoveMode::small;
const double kPredictFreq = 50.0;
const double kPredictDeltaT = 0.2;

string trim(const string& text) {
    const size_t first = text.find_first_not_of(" \t\r\n");
    if (first == string::npos) {
        return "";
    }
    const size_t last = text.find_last_not_of(" \t\r\n");
    return text.substr(first, last - first + 1);
}

string normalize_path_input(const string& text) {
    string path = trim(text);
    if (path.size() >= 2) {
        const bool has_double_quotes = path.front() == '"' && path.back() == '"';
        const bool has_single_quotes = path.front() == '\'' && path.back() == '\'';
        if (has_double_quotes || has_single_quotes) {
            path = path.substr(1, path.size() - 2);
        }
    }

    const string file_scheme = "file:///";
    if (path.rfind(file_scheme, 0) == 0) {
        path = path.substr(file_scheme.size());
    }

    string collapsed;
    collapsed.reserve(path.size());
    for (size_t i = 0; i < path.size(); ++i) {
        if (path[i] == '\\' && i + 1 < path.size() && path[i + 1] == '\\') {
            collapsed.push_back('\\');
            ++i;
            continue;
        }
        collapsed.push_back(path[i]);
    }
    path = collapsed;

    if (path.size() >= 3 && path[0] == '/' && std::isalpha(static_cast<unsigned char>(path[1])) && path[2] == ':') {
        path = path.substr(1);
    }

    return path;
}

vector<string> get_video_path_candidates(const string& raw_input) {
    vector<string> candidates;
    const string normalized = normalize_path_input(raw_input);

    auto push_unique = [&candidates](const string& value) {
        if (value.empty()) {
            return;
        }
        if (find(candidates.begin(), candidates.end(), value) == candidates.end()) {
            candidates.push_back(value);
        }
    };

    push_unique(normalized);

    string slash_style = normalized;
    replace(slash_style.begin(), slash_style.end(), '\\', '/');
    push_unique(slash_style);

    string backslash_style = normalized;
    replace(backslash_style.begin(), backslash_style.end(), '/', '\\');
    push_unique(backslash_style);

    return candidates;
}

bool open_video_capture(const string& input_path, VideoCapture& cap, string& resolved_path) {
    const vector<string> candidates = get_video_path_candidates(input_path);
    for (const auto& candidate : candidates) {
        if (cap.open(candidate)) {
            resolved_path = candidate;
            return true;
        }
    }
    return false;
}

bool try_parse_double(const string& text, double& value) {
    try {
        size_t parsed_chars = 0;
        value = stod(text, &parsed_chars);
        return parsed_chars == text.size();
    } catch (...) {
        return false;
    }
}

double clamp_playback_speed(double speed) {
    return std::max(0.25, std::min(8.0, speed));
}

int get_wait_delay_ms(double playback_speed, double source_fps) {
    const double fps = (source_fps > 1e-3) ? source_fps : 30.0;
    const double base_delay_ms = 1000.0 / fps;
    if (playback_speed >= 1.0) {
        return std::max(1, static_cast<int>(std::round(base_delay_ms)));
    }
    return std::max(1, static_cast<int>(std::round(base_delay_ms / playback_speed)));
}

Size get_initial_window_size(const Mat& image) {
    const int max_width = 1400;
    const int max_height = 900;

    if (image.empty()) {
        return Size(max_width, max_height);
    }

    const double width_scale = static_cast<double>(max_width) / image.cols;
    const double height_scale = static_cast<double>(max_height) / image.rows;
    const double scale = std::min(1.0, std::min(width_scale, height_scale));

    return Size(std::max(1, static_cast<int>(image.cols * scale)),
                std::max(1, static_cast<int>(image.rows * scale)));
}

void prepare_resizable_window(const string& window_name, const Mat& image) {
    namedWindow(window_name, WINDOW_NORMAL | WINDOW_KEEPRATIO);
    const Size initial_size = get_initial_window_size(image);
    resizeWindow(window_name, initial_size.width, initial_size.height);
}

}  // namespace

int main(int argc, char** argv) {
    string video_path = "/home/zhongxiangyu/BUFF_Tracker/能量机关视频.mp4";
    if (argc >= 2) {
        video_path = normalize_path_input(argv[1]);
    }

    cout << "Video path (press ENTER to use default):" << endl;
    cout << "  [" << video_path << "]" << endl;
    cout << "> ";
    string path_input;
    getline(cin, path_input);
    path_input = normalize_path_input(path_input);
    if (!path_input.empty()) {
        video_path = path_input;
    }

    double playback_speed = 1.0;
    cout << "Initial playback speed (0.25~8.0, default 1.0):" << endl;
    cout << "> ";
    string speed_input;
    getline(cin, speed_input);
    speed_input = trim(speed_input);
    if (!speed_input.empty()) {
        double parsed_speed = 1.0;
        if (try_parse_double(speed_input, parsed_speed)) {
            playback_speed = clamp_playback_speed(parsed_speed);
        } else {
            cout << "Invalid speed input. Fallback to 1.0x." << endl;
        }
    }

    cout << "Playback hotkeys (after ROI): [ slower, ] faster, 0 reset, SPACE pause/play, q or ESC quit." << endl;
    cout << "Note: speed > 1.0x uses frame skipping for fast-forward." << endl;

    VideoCapture cap;
    string resolved_video_path;
    if (!open_video_capture(video_path, cap, resolved_video_path)) {
        cout << "Error opening video: " << video_path << endl;
        cout << "Try full absolute Windows path, for example: E:\\RM\\rm_vision\\examples\\3_12mm_red_dark\\12mm_red_dark.mp4" << endl;
        return -1;
    }
    cout << "Using video: " << resolved_video_path << endl;
    double source_fps = cap.get(CAP_PROP_FPS);
    if (source_fps <= 1e-3) {
        source_fps = 30.0;
    }
    cout << "Source FPS: " << format("%.2f", source_fps) << endl;

    namedWindow("HSV_Tuner", WINDOW_NORMAL);
    resizeWindow("HSV_Tuner", 350, 450);
    createTrackbar("H Min", "HSV_Tuner", &h_min, 180, on_trackbar);
    createTrackbar("H Max", "HSV_Tuner", &h_max, 180, on_trackbar);
    createTrackbar("S Min", "HSV_Tuner", &s_min, 255, on_trackbar);
    createTrackbar("S Max", "HSV_Tuner", &s_max, 255, on_trackbar);
    createTrackbar("V Min", "HSV_Tuner", &v_min, 255, on_trackbar);
    createTrackbar("V Max", "HSV_Tuner", &v_max, 255, on_trackbar);
    createTrackbar("Morph_K", "HSV_Tuner", &morph_kernel, 20, on_trackbar); // max kernel 20
    
    // Set trackbars to defaults suitable for RED
    // For red, hue is usually around 0-10 or 160-180.
    setTrackbarPos("H Min", "HSV_Tuner", h_min);
    setTrackbarPos("H Max", "HSV_Tuner", h_max);
    setTrackbarPos("S Min", "HSV_Tuner", s_min);
    setTrackbarPos("S Max", "HSV_Tuner", s_max);
    setTrackbarPos("V Min", "HSV_Tuner", v_min);
    setTrackbarPos("V Max", "HSV_Tuner", v_max);
    setTrackbarPos("Morph_K", "HSV_Tuner", morph_kernel);

    RuneDetector detector;
    Mat frame;

    // Preview frames first, then press SPACE to start ROI selection.
    cap >> frame;
    if (frame.empty()) {
        cout << "Error: Failed to read frame for ROI preview" << endl;
        return -1;
    }

    prepare_resizable_window("ROI Preview", frame);
    cout << "ROI Preview: press SPACE to freeze and select boxes." << endl;
    cout << "Preview hotkeys: [ slower, ] faster, 0 reset, q/ESC quit." << endl;

    double preview_skip_accumulator = 0.0;
    while (true) {
        Mat preview = frame.clone();
        putText(preview,
                format("Preview Speed: %.2fx", playback_speed),
                Point(20, 30),
                FONT_HERSHEY_SIMPLEX,
                0.8,
                Scalar(0, 255, 255),
                2);
        putText(preview,
                "SPACE select ROI  [ ] speed  0 reset  q/ESC quit",
                Point(20, 60),
                FONT_HERSHEY_SIMPLEX,
                0.6,
                Scalar(0, 255, 255),
                2);
        imshow("ROI Preview", preview);

        const int preview_delay = get_wait_delay_ms(playback_speed, source_fps);
        const char key = static_cast<char>(waitKey(preview_delay));
        if (key == ' ' ) {
            break;
        }
        if (key == 27 || key == 'q') {
            return 0;
        }
        if (key == '[' || key == '-') {
            playback_speed = clamp_playback_speed(playback_speed / 1.25);
            cout << "Preview speed: " << format("%.2fx", playback_speed) << endl;
        } else if (key == ']' || key == '=' || key == '+') {
            playback_speed = clamp_playback_speed(playback_speed * 1.25);
            cout << "Preview speed: " << format("%.2fx", playback_speed) << endl;
        } else if (key == '0') {
            playback_speed = 1.0;
            cout << "Preview speed reset to 1.00x" << endl;
        }

        if (playback_speed > 1.0) {
            preview_skip_accumulator += (playback_speed - 1.0);
            int frames_to_skip = static_cast<int>(preview_skip_accumulator);
            preview_skip_accumulator -= frames_to_skip;
            while (frames_to_skip > 0) {
                if (!cap.grab()) {
                    cap.set(CAP_PROP_POS_FRAMES, 0);
                    break;
                }
                --frames_to_skip;
            }
        } else {
            preview_skip_accumulator = 0.0;
        }

        cap >> frame;
        if (frame.empty()) {
            cap.set(CAP_PROP_POS_FRAMES, 0);
            cap >> frame;
            if (frame.empty()) {
                cout << "Error: Failed to read frame during ROI preview" << endl;
                return -1;
            }
        }
    }
    destroyWindow("ROI Preview");

    // Manual ROI selection (matches the Python prototype behavior)
    cout << "Please select the center R bounding box, then press SPACE or ENTER." << endl;
    cout << "Tip: You can drag the window border to resize while selecting ROI." << endl;
    prepare_resizable_window("Select R", frame);
    Rect r_rect = selectROI("Select R", frame, false, false);
    
    cout << "Please select the target Fan Blade bounding box, then press SPACE or ENTER." << endl;
    prepare_resizable_window("Select Fan Blade", frame);
    Rect fan_rect = selectROI("Select Fan Blade", frame, false, false);
    
    destroyWindow("Select R");
    destroyWindow("Select Fan Blade");

    detector.init(r_rect, fan_rect);

    const ClockMode clock_mode = (kPredictColor == "red") ? ClockMode::clockwise : ClockMode::anticlockwise;
    AngleObserver angle_observer(clock_mode);
    SmallPredictor small_predictor(kPredictDeltaT, kPredictFreq);
    BigPredictor big_predictor(kPredictDeltaT, kPredictFreq);
    const int predict_interval = max(1, static_cast<int>(ceil(kPredictFreq * kPredictDeltaT)));
    vector<Point2f> predict_history;

    prepare_resizable_window("Original", frame);

    bool paused = false;
    double frame_skip_accumulator = 0.0;
    double runtime_fps = 0.0;
    double detect_ms_ema = 0.0;
    int64 last_loop_tick = getTickCount();
    double perf_print_acc = 0.0;

    while (true) {
        if (!paused) {
            cap >> frame;
            if (frame.empty()) {
                
                // loop back if empty 
                cap.set(CAP_PROP_POS_FRAMES, 0);
                continue;
            }

            if (playback_speed > 1.0) {
                frame_skip_accumulator += (playback_speed - 1.0);
                int frames_to_skip = static_cast<int>(frame_skip_accumulator);
                frame_skip_accumulator -= frames_to_skip;

                while (frames_to_skip > 0) {
                    if (!cap.grab()) {
                        cap.set(CAP_PROP_POS_FRAMES, 0);
                        break;
                    }
                    --frames_to_skip;
                }
            } else {
                frame_skip_accumulator = 0.0;
            }
        }
        
        // Pass the dynamically tuned HSV parameters down
        detector.param.lowerLimit = Scalar(h_min, s_min, v_min);
        detector.param.upperLimit = Scalar(h_max, s_max, v_max);
        
        // Ensure kernel size >= 1
        detector.param.kernel_size = max(1, morph_kernel);

        const int64 detect_begin = getTickCount();
        const bool detect_ok = detector.update(frame, true);
        const double detect_ms = (getTickCount() - detect_begin) * 1000.0 / getTickFrequency();
        if (detect_ms_ema <= 0.0) {
            detect_ms_ema = detect_ms;
        } else {
            detect_ms_ema = detect_ms_ema * 0.9 + detect_ms * 0.1;
        }
        if (detect_ok) {
            const Point2f rel = detector.fanBladeBox.center_2f() - detector.R_Box.center_2f();
            const double angle = angle_observer.update(rel.x, rel.y, detector.radius);

            pair<bool, double> pred_result;
            if (kPredictMoveMode == MoveMode::small) {
                pred_result = small_predictor.update(angle);
            } else {
                pred_result = big_predictor.update(angle);
            }

            if (pred_result.first) {
                const double predict_theta = trans_angle(rel.x, rel.y) + pred_result.second;
                const Point2f r_center = detector.R_Box.center_2f();
                const double px = cos(predict_theta) * detector.radius + r_center.x;
                const double py = sin(predict_theta) * detector.radius + r_center.y;
                const Point2f predict_point(static_cast<float>(px), static_cast<float>(py));
                predict_history.push_back(predict_point);

                circle(detector.debug_frame,
                       Point(static_cast<int>(predict_point.x), static_cast<int>(predict_point.y)),
                       10,
                       Scalar(0, 255, 0),
                       -1);
                putText(detector.debug_frame,
                        "now predict",
                        Point(static_cast<int>(predict_point.x), static_cast<int>(predict_point.y)),
                        FONT_HERSHEY_SIMPLEX,
                        0.75,
                        Scalar(0, 0, 255),
                        2);

                if (static_cast<int>(predict_history.size()) >= predict_interval) {
                    const Point2f before_predict = predict_history[predict_history.size() - predict_interval];
                    circle(detector.debug_frame,
                           Point(static_cast<int>(before_predict.x), static_cast<int>(before_predict.y)),
                           10,
                           Scalar(0, 255, 0),
                           -1);
                    putText(detector.debug_frame,
                            "before predict",
                            Point(static_cast<int>(before_predict.x), static_cast<int>(before_predict.y)),
                            FONT_HERSHEY_SIMPLEX,
                            0.75,
                            Scalar(0, 0, 255),
                            2);
                }
            }
        }

        const int64 now_tick = getTickCount();
        const double loop_dt = (now_tick - last_loop_tick) / getTickFrequency();
        last_loop_tick = now_tick;
        if (loop_dt > 1e-6) {
            const double instant_fps = 1.0 / loop_dt;
            if (runtime_fps <= 0.0) {
                runtime_fps = instant_fps;
            } else {
                runtime_fps = runtime_fps * 0.9 + instant_fps * 0.1;
            }

            perf_print_acc += loop_dt;
            if (perf_print_acc >= 1.0) {
                const double target_budget_ms = 1000.0 / std::max(1e-3, source_fps * std::max(0.25, playback_speed));
                const bool is_bottleneck = detect_ms_ema > target_budget_ms;
                cout << "[Perf] SrcFPS " << format("%.2f", source_fps)
                     << " | RunFPS " << format("%.2f", runtime_fps)
                     << " | Detect " << format("%.1f", detect_ms_ema) << "ms"
                     << " | Budget " << format("%.1f", target_budget_ms) << "ms"
                     << " | " << (is_bottleneck ? "BOTTLENECK" : "OK") << endl;
                perf_print_acc = 0.0;
            }
        }

        putText(detector.debug_frame,
                format("Speed: %.2fx", playback_speed),
                Point(20, 30),
                FONT_HERSHEY_SIMPLEX,
                0.8,
                Scalar(0, 255, 255),
                2);
        putText(detector.debug_frame,
                "[ ] speed  0 reset  SPACE pause  q/ESC quit",
                Point(20, 60),
                FONT_HERSHEY_SIMPLEX,
                0.6,
                Scalar(0, 255, 255),
                2);
        rectangle(detector.debug_frame,
                  Point(12, 70),
                  Point(560, 104),
                  Scalar(0, 0, 0),
                  FILLED);
        const double target_budget_ms = 1000.0 / std::max(1e-3, source_fps * std::max(0.25, playback_speed));
        const bool is_bottleneck = detect_ms_ema > target_budget_ms;
        putText(detector.debug_frame,
                format("Src %.1f | Run %.1f | Det %.1fms | Budget %.1fms | %s",
                       source_fps,
                       runtime_fps,
                       detect_ms_ema,
                       target_budget_ms,
                       is_bottleneck ? "BOTTLENECK" : "OK"),
                Point(20, 93),
                FONT_HERSHEY_SIMPLEX,
                0.6,
                Scalar(255, 255, 255),
                2);
        
        imshow("Original", detector.debug_frame);

        const int wait_delay = paused ? 30 : get_wait_delay_ms(playback_speed, source_fps);
        char c = (char)waitKey(wait_delay);
        if (c == 27 || c == 'q') {
            break;
        } else if (c == ' ') {
            paused = !paused; // Pause/Play with spacebar
        } else if (c == '[' || c == '-') {
            playback_speed = clamp_playback_speed(playback_speed / 1.25);
            cout << "Playback speed: " << format("%.2fx", playback_speed) << endl;
        } else if (c == ']' || c == '=' || c == '+') {
            playback_speed = clamp_playback_speed(playback_speed * 1.25);
            cout << "Playback speed: " << format("%.2fx", playback_speed) << endl;
        } else if (c == '0') {
            playback_speed = 1.0;
            cout << "Playback speed reset to 1.00x" << endl;
        }
    }

    cap.release();
    destroyAllWindows();
    return 0;
}