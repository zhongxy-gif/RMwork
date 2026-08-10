# color_detection 使用说明

## 1. 编译

```bash
cd ~/ros2_ws
colcon build
```

## 2. 配置环境

```bash
source install/setup.bash
```

## 3. 运行节点

需要打开 **两个终端**，分别执行：

### 终端 1：启动摄像头发布者（采集并发布图像）

```bash
ros2 run color_detection camera_publisher
```

### 终端 2：启动颜色检测器（订阅图像并显示结果）

```bash
ros2 run color_detection color_detector
```

## 4. 运行效果

- **camera_publisher**：从默认摄像头（设备索引 0）采集画面，发布到 `camera/image_raw` 话题，终端会打印 `Publishing camera image`
- **color_detector**：订阅 `camera/image_raw` 话题，弹出两个窗口分别显示：
  - `Blue Detection (HSV Threshold)` —— 蓝色检测结果
  - `Red Detection (RGB Threshold)` —— 红色检测结果

## 5. 摄像头索引说明

如果默认摄像头（0）无法使用，可修改 `camera_publisher.py` 第 13 行：

```python
self.cap = cv2.VideoCapture(0)   # 0 → 改为其他索引(1,2,...) 或 视频文件路径
```

## 6. 话题关系

```
camera_publisher  ──camera/image_raw──▶  color_detector
     (发布者)                              (订阅者)
```

## 7. 退出

在任意运行节点的终端按 `Ctrl+C` 即可停止。