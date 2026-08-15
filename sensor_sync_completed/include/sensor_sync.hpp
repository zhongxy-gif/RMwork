#pragma once // 防止头文件被重复包含

#include <cstdint> // 引入固定宽度整数类型(std::uint32_t)
#include <algorithm> // 引入排序算法 std::sort
#include <cstdlib> // 引入 std::llabs 计算绝对值
#include <iterator> // 引入迭代器相关工具
#include <optional> // 引入 std::optional 表示可能为空的值
#include <stdexcept> // 引入异常类 std::invalid_argument
#include <vector> // 引入 std::vector 容器

enum class SensorType { CAMERA, IMU }; // 传感器类型枚举：相机或IMU

struct SensorData { // 传感器数据包结构体
  SensorType type; // 传感器类型
  std::uint32_t sequence; // 数据包序号(0~4294967295)
  long long timestamp_us; // 时间戳，单位微秒
};

struct SyncResult { // 同步匹配结果结构体
  SensorData camera; // 对应的相机数据包
  std::optional<SensorData> imu; // 匹配到的IMU数据包，可能为空
  long long delta_us = 0; // 相机与IMU的时间差绝对值
};

inline std::vector<SyncResult> synchronize( // 同步匹配函数，返回所有相机的匹配结果
    std::vector<SensorData> cameras, // 传入相机数据包列表(按值传递以便排序)
    std::vector<SensorData> imus, // 传入IMU数据包列表(按值传递以便排序)
    long long max_delta_us) { // 最大允许时间差，单位微秒
  if (max_delta_us < 0) throw std::invalid_argument("max_delta_us must not be negative"); // 参数合法性校验
  auto by_time = [](const SensorData& a, const SensorData& b) { // 排序比较函数：先按时间戳升序，再按序号升序
    return a.timestamp_us != b.timestamp_us ? a.timestamp_us < b.timestamp_us // 时间戳不同则按时戳比较
                                             : a.sequence < b.sequence; // 时间戳相同则按序号比较
  };
  std::sort(cameras.begin(), cameras.end(), by_time); // 将相机数据按时间戳排序
  std::sort(imus.begin(), imus.end(), by_time); // 将IMU数据按时间戳排序
  std::vector<SyncResult> results; // 存储匹配结果的容器
  results.reserve(cameras.size()); // 预留空间，避免反复分配内存
  auto right = imus.begin(); // 双指针：指向IMU中第一个时间戳>=当前相机的位置
  for (const auto& camera : cameras) { // 遍历每个相机包进行匹配
    while (right != imus.end() && right->timestamp_us < camera.timestamp_us) ++right; // 跳过时间戳小于当前相机的IMU
    const SensorData* best = nullptr; // 记录当前最佳匹配的IMU指针
    auto consider = [&](const SensorData& imu) { // 候选评估函数：比较给定IMU与当前相机的匹配度
      const long long delta = std::llabs(camera.timestamp_us - imu.timestamp_us); // 计算时间差绝对值
      if (best == nullptr || delta < std::llabs(camera.timestamp_us - best->timestamp_us) || // 更优候选：差值更小
          (delta == std::llabs(camera.timestamp_us - best->timestamp_us) && // 差值相同情况下
           (imu.timestamp_us < best->timestamp_us || // 选时间戳更早的IMU
            (imu.timestamp_us == best->timestamp_us && imu.sequence < best->sequence)))) best = &imu; // 时间戳也相同则选序号更小的
    };
    if (right != imus.end()) consider(*right); // 评估右指针所指的IMU(相机时间戳之后的第一个)
    if (right != imus.begin()) consider(*std::prev(right)); // 评估右指针前一个IMU(相机时间戳之前的最后一个)
    if (best != nullptr && std::llabs(camera.timestamp_us - best->timestamp_us) <= max_delta_us) // 最佳候选存在且差值在允许范围内
      results.push_back({camera, *best, std::llabs(camera.timestamp_us - best->timestamp_us)}); // 记录匹配成功结果
    else results.push_back({camera, std::nullopt, 0}); // 无合适匹配，记录为UNMATCHED
  }
  return results; // 返回所有匹配结果
}