#pragma once // 防止头文件被重复包含

#include <cstddef> // 引入 std::size_t 类型
#include <condition_variable> // 条件变量,用于线程间的阻塞等待和唤醒
#include <deque> // 双端队列,作为队列的底层实现
#include <mutex> // 互斥锁,保护队列的并发访问
#include <stdexcept> // 异常类,用于抛出错误
#include <utility> // 引入 std::move 转移语义

template <typename T> // 模板类,T为队列中存储的数据类型
class ThreadSafeQueue { // 线程安全队列类
 public:
  explicit ThreadSafeQueue(std::size_t capacity) : capacity_(capacity) { // 构造函数,初始化队列容量
    if (capacity_ == 0) { // 容量必须大于0
      throw std::invalid_argument("queue capacity must be greater than zero"); // 容量为0则抛异常
    }
  }

  ThreadSafeQueue(const ThreadSafeQueue&) = delete; // 禁止拷贝构造
  ThreadSafeQueue& operator=(const ThreadSafeQueue&) = delete; // 禁止拷贝赋值

  bool push(T value) { // 生产者入队方法:将数据放入队列,队列满时阻塞
    std::unique_lock<std::mutex> lock(mutex_); // 加锁,保护队列状态
    not_full_.wait(lock, [this] { return closed_ || queue_.size() < capacity_; }); // 等待直到队列未满或已关闭
    if (closed_) return false; // 如果队列已关闭,放弃入队,返回false
    queue_.push_back(std::move(value)); // 将数据移到队列尾部
    not_empty_.notify_one(); // 唤醒一个等待的消费者(队列现在非空了)
    return true; // 入队成功,返回true
  }

  bool pop(T& value) { // 消费者出队方法:从队列取数据,队列空时阻塞
    std::unique_lock<std::mutex> lock(mutex_); // 加锁,保护队列状态
    not_empty_.wait(lock, [this] { return closed_ || !queue_.empty(); }); // 等待直到队列非空或已关闭
    if (queue_.empty()) return false; // 队列仍为空(已关闭),返回false
    value = std::move(queue_.front()); // 取出队列头部的数据
    queue_.pop_front(); // 从队列中移除头部元素
    not_full_.notify_one(); // 唤醒一个等待的生产者(队列现在有空间了)
    return true; // 出队成功,返回true
  }

  void close() { // 关闭队列:标记不再接受新数据,唤醒所有等待线程
    {
      std::lock_guard<std::mutex> lock(mutex_); // 加锁,保护 closed_ 标志
      closed_ = true; // 标记队列为关闭状态
    } // 离开作用域,自动解锁
    not_empty_.notify_all(); // 唤醒所有等待数据的消费者
    not_full_.notify_all(); // 唤醒所有等待空间的生产者
  }

  bool empty() const { // 判断队列是否为空
    std::lock_guard<std::mutex> lock(mutex_); // 加锁保护
    return queue_.empty(); // 返回底层队列是否为空
  }

  std::size_t size() const { // 获取队列当前元素数量
    std::lock_guard<std::mutex> lock(mutex_); // 加锁保护
    return queue_.size(); // 返回底层队列的元素个数
  }

 private:
  const std::size_t capacity_; // 队列最大容量(创建后不可变)
  std::deque<T> queue_; // 底层存储容器:双端队列
  bool closed_ = false; // 队列关闭标志
  mutable std::mutex mutex_; // 互斥锁,保护所有共享状态(声明为mutable以便在const方法中使用)
  std::condition_variable not_empty_; // 条件变量:队列非空时通知消费者
  std::condition_variable not_full_; // 条件变量:队列未满时通知生产者
};