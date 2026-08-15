# 流程图

## main.cpp 主程序流程

```
开始
  |
  v
读取配置文件(config/sensor_sync.conf)
  |
  v
创建线程安全队列
  |
  v
启动 3 个线程:
  |-- 相机生产线程  -- 读文件,把数据放进队列
  |-- IMU生产线程  -- 读文件,把数据放进队列
  |-- 消费者线程    -- 从队列拿数据,按类型分开
  |
  v
等待两个生产者线程结束
  |
  v
关闭队列(告诉消费者:没有新数据了)
  |
  v
等待消费者线程结束
  |
  v
调用 synchronize 做时间戳匹配
  |
  v
把结果写入 data/result.txt
  |
  v
结束
```

---

## thread_safe_queue.hpp 队列操作流程

### push(生产者放数据)

```
生产者线程调用 push(data)
  |
  v
加锁
  |
  v
队列满了 且 未关闭?
  |是                    |否
  v                      v
等待(其他线程pop后唤醒)   队列关闭了?
                           |是          |否
                           v           v
                       返回 false    数据入队
                                      |
                                      v
                                   通知消费者
                                      |
                                      v
                                   返回 true
```

### pop(消费者拿数据)

```
消费者线程调用 pop(variable)
  |
  v
加锁
  |
  v
队列空 且 未关闭?
  |是                    |否
  v                      v
等待(生产者push后唤醒)    队列还是空的?
                           |是          |否
                           v           v
                       返回 false    数据出队
                                      |
                                      v
                                   通知生产者
                                      |
                                      v
                                   返回 true
```

### close(关闭队列)

```
主线程调用 close()
  |
  v
加锁,设置 closed = true
  |
  v
释放锁
  |
  v
唤醒所有等待的线程
```

---

## sensor_sync.hpp synchronize 匹配算法

### 整体思路

```
把相机和IMU数据各自按时间戳排序
  |
  v
遍历每个相机,在已排序的IMU中
  只找它前后相邻的两个IMU来比较
  (因为排好序了,最接近的肯定在旁边)
  |
  v
时间差 <= max_delta_us(300)  → 匹配成功
时间差 > max_delta_us(300)   → 匹配失败 UNMATCHED
  |
  v
返回所有匹配结果
```

### 用实际数据走一遍

```
IMU数据(按时间排序):
  IMU0(800)  IMU1(1200)  IMU2(1900)  IMU3(3100)  IMU4(4200)

相机数据(按时间排序):
  C0(1000)   C1(2000)    C2(3000)    C3(5000)

处理 C0(1000):
  往前看 IMU0(800)   差200
  往后看 IMU1(1200)  差200
  两个一样近,选更早的 IMU0
  → 匹配成功, DELTA 200

处理 C1(2000):
  往前看 IMU2(1900)  差100  ← 更近
  往后看 IMU3(3100)  差1100
  → 匹配成功 IMU2, DELTA 100

处理 C2(3000):
  往前看 IMU3(3100)  差100  ← 更近
  往后看 IMU4(4200)  差1200
  → 匹配成功 IMU3, DELTA 100

处理 C3(5000):
  往前看 IMU4(4200)  差800
  往后看 没有了
  800 > 300,超过限制
  → UNMATCHED

最终结果:
  CAMERA 0 IMU 0 DELTA 200
  CAMERA 1 IMU 2 DELTA 100
  CAMERA 2 IMU 3 DELTA 100
  CAMERA 3 UNMATCHED
```