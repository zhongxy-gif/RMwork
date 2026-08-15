#include <filesystem> // 文件系统操作,用于路径拼接和目录处理
#include <climits> // 引入 SIZE_MAX 和 LLONG_MAX 常量
#include <cstdint> // 引入固定宽度整数类型(std::uint32_t等)
#include <fstream> // 文件输入输出流
#include <iostream> // 标准输入输出(cout/cerr)
#include <mutex> // 互斥锁,保护多线程共享数据
#include <sstream> // 字符串流,用于解析文本行
#include <string> // 字符串类型
#include <thread> // 线程类 std::thread
#include <unordered_map> // 哈希表,用于键值对存储配置项
#include <vector> // 动态数组容器

#include "sensor_sync.hpp" // 传感器数据结构和同步匹配算法
#include "thread_safe_queue.hpp" // 线程安全队列

namespace { // 匿名命名空间,限制内部函数和结构体的作用域在本文件内

struct Config { // 配置结构体,保存从配置文件读取的所有参数
  std::size_t capacity; // 队列容量
  long long max_delta; // 最大允许时间差,单位微秒
  std::filesystem::path camera; // 相机数据文件路径
  std::filesystem::path imu; // IMU数据文件路径
  std::filesystem::path output; // 结果输出文件路径
};

std::string trim(std::string s) { // 去除字符串首尾的空白字符(空格、制表符、回车、换行)
  const auto first = s.find_first_not_of(" \t\r\n"); // 找到第一个非空白字符的位置
  if (first == std::string::npos) return ""; // 全是空白,返回空字符串
  return s.substr(first, s.find_last_not_of(" \t\r\n") - first + 1); // 返回首尾空白之间的子串
}

Config read_config(const std::filesystem::path& file) { // 从配置文件读取参数并返回 Config 结构体
  std::ifstream in(file); if (!in) throw std::runtime_error("cannot open config: " + file.string()); // 打开配置文件,失败则抛异常
  std::unordered_map<std::string, std::string> v; std::string line; int n = 0; // 用哈希表存储键值对,n记录行号
  while (std::getline(in, line)) { ++n; line = trim(line); if (line.empty() || line[0] == '#') continue; // 逐行读取,跳过空行和注释行
    const auto pos = line.find('='); if (pos == std::string::npos || line.find('=', pos + 1) != std::string::npos) // 校验必须有且只有一个等号
      throw std::runtime_error("invalid config line " + std::to_string(n)); // 格式错误则抛异常,提示行号
    v[trim(line.substr(0,pos))] = trim(line.substr(pos+1)); // 以等号为界,提取key和value,存入哈希表
  }
  for (const char* k : {"queue_capacity", "max_delta_us", "camera_file", "imu_file", "output_file"}) // 遍历必须存在的5个配置键
    if (!v.count(k) || v[k].empty()) throw std::runtime_error(std::string("missing config key: ") + k); // 缺失或为空则抛异常
  auto unsigned_value = [](const std::string& text, const char* key) { size_t p = 0; unsigned long long x; // 定义lambda:将字符串解析为无符号整数
    try { x = std::stoull(text, &p); } catch (...) { throw std::runtime_error(std::string("invalid ") + key); } // stoull转换失败则抛异常
    if (p != text.size()) throw std::runtime_error(std::string("invalid ") + key); // 确保整串都被解析(排除多余字符)
    return x; }; // 返回解析出的整数值
  const auto cap = unsigned_value(v["queue_capacity"], "queue_capacity"); // 解析队列容量
  const auto delta = unsigned_value(v["max_delta_us"], "max_delta_us"); // 解析最大时间差
  if (cap == 0 || cap > static_cast<unsigned long long>(SIZE_MAX) || delta > static_cast<unsigned long long>(LLONG_MAX)) // 校验数值范围
    throw std::runtime_error("configuration value out of range"); // 超出范围则抛异常
  const auto base = file.parent_path(); // 获取配置文件所在目录,用于解析相对路径
  return {static_cast<std::size_t>(cap), static_cast<long long>(delta), base/v["camera_file"], base/v["imu_file"], base/v["output_file"]}; // 用配置值构造并返回 Config
}

void produce(const std::filesystem::path& file, SensorType type, ThreadSafeQueue<SensorData>& queue) { // 生产者函数:从文件读取数据并写入队列
  std::ifstream in(file); if (!in) throw std::runtime_error("cannot open data file: " + file.string()); // 打开数据文件,失败则抛异常
  std::string line; int n = 0; // 逐行读取,n记录行号
  while (std::getline(in,line)) { ++n; std::istringstream ss(line); long long sequence, timestamp; std::string extra; // 用字符串流解析每一行
    if (!(ss >> sequence >> timestamp) || (ss >> extra) || sequence < 0 || sequence > UINT32_MAX || timestamp < 0) // 校验格式和数值范围
      throw std::runtime_error("invalid packet in " + file.string() + ", line " + std::to_string(n)); // 格式错误则抛异常
    if (!queue.push({type, static_cast<std::uint32_t>(sequence), timestamp})) return; // 将数据包推入队列,队列关闭则提前退出
  }
}
} // 匿名命名空间结束

int main(int argc, char* argv[]) { // 主函数
  if (argc != 2) { std::cerr << "Usage: " << argv[0] << " <config-file>\n"; return 1; } // 参数个数不对,打印用法并退出
  try { // 整个主逻辑放在 try 块中,捕获任何异常
    const Config cfg = read_config(argv[1]); ThreadSafeQueue<SensorData> queue(cfg.capacity); // 读取配置,创建有界队列
    std::vector<SensorData> cameras, imus; std::mutex error_mutex; std::string error; // 存储两类数据包,错误互斥锁,错误信息
    auto producer = [&](const std::filesystem::path& f, SensorType type) { try { produce(f,type,queue); } // 定义生产者lambda:调用produce,内部捕获异常
      catch (const std::exception& e) { std::lock_guard<std::mutex> lock(error_mutex); if (error.empty()) error=e.what(); } }; // 捕获异常时记录错误信息(只记录第一个)
    std::thread camera_thread(producer, cfg.camera, SensorType::CAMERA); // 启动相机生产线程
    std::thread imu_thread(producer, cfg.imu, SensorType::IMU); // 启动IMU生产线程
    std::thread consumer([&] { SensorData packet; while (queue.pop(packet)) // 启动消费者线程:持续从队列取数据
      (packet.type == SensorType::CAMERA ? cameras : imus).push_back(packet); }); // 按类型分别存入 cameras 或 imus
    camera_thread.join(); imu_thread.join(); queue.close(); consumer.join(); // 等待两个生产者结束→关闭队列→等待消费者处理完剩余数据
    if (!error.empty()) { std::cerr << error << '\n'; return 1; } // 如果生产者出错,打印错误并退出
    std::ofstream out(cfg.output); if (!out) throw std::runtime_error("cannot open output: " + cfg.output.string()); // 打开输出文件
    for (const auto& r : synchronize(std::move(cameras), std::move(imus), cfg.max_delta)) { // 调用 synchronize 进行时间戳匹配
      out << "CAMERA " << r.camera.sequence; // 输出相机序号
      if (r.imu) out << " IMU " << r.imu->sequence << " DELTA " << r.delta_us; // 匹配成功则输出IMU序号和时间差
      else out << " UNMATCHED"; // 匹配失败则输出 UNMATCHED
      out << '\n'; // 换行
    }
    out.close(); // 关闭输出文件
    std::cout << "Done. Result written to " << cfg.output << '\n'; // 打印完成提示
  } catch (const std::exception& e) { std::cerr << "Error: " << e.what() << '\n'; return 1; } // 捕获顶层异常,打印错误并退出
  return 0; // 正常退出
}