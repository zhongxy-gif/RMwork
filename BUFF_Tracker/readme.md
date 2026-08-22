# BUFF Tracker 运行指南

## 这是什么？

这是一个 **RoboMaster 能量机关（Rune）检测器**。它的工作流程是：

1. **你手动在视频第一帧上框出两个区域**：
   - **R 中心标志**（能量机关中心的那个"R"图案）
   - **扇叶**（围绕 R 旋转的彩色叶片之一）
2. 之后程序会**自动**在每一帧中跟踪扇叶的位置和旋转角度
3. 它还会**预测**扇叶下一时刻的位置，用于自动打击

简单说：**第一帧需要你手动标一下位置，之后全自动。**

---

## 编译步骤

### 方式一：VSCode（推荐）

1. 安装扩展：`C/C++` 和 `CMake Tools`
2. 用 VSCode 打开项目文件夹：`/home/zhongxiangyu/BUFF_Tracker`
3. 打开命令面板（Ctrl+Shift+P），依次执行：
   - `CMake: Select Configure Preset` → 选择 `Linux GCC x64 Debug`
   - `CMake: Configure`
   - `CMake: Build`

### 方式二：终端命令

```bash
cd /home/zhongxiangyu/BUFF_Tracker
mkdir -p build && cd build
cmake ..
make -j$(nproc)
```

编译完成后，可执行文件在：`build/rune_detector_test`

---

## 运行步骤

```bash
cd /home/zhongxiangyu/BUFF_Tracker/build
./rune_detector_test
```

**重要提示**：在运行主程序前，建议先使用调参工具调节HSV参数：
```bash
python3 tuner.py /home/zhongxiangyu/BUFF_Tracker/能量机关视频.mp4
```
调节完成后参数会保存到 `hsv_params.json` 文件中，可以参考其中的数值来调整主程序的参数。

运行后会有 **三步交互**，请按顺序操作：

### 第 1 步：视频路径

```
Video path (press ENTER to use default):
  [/home/zhongxiangyu/BUFF_Tracker/能量机关视频.mp4]
>
```

- **直接按 ENTER** → 使用默认路径
- 输入其他路径 → 加载你指定的视频

### 第 2 步：播放速度

```
Initial playback speed (0.25~8.0, default 1.0):
>
```

- **直接按 ENTER** → 使用 1.0x 正常速度
- 输入数字 → 指定播放速度（0.25 最慢，8.0 最快）

### 第 3 步：找到合适的帧并标框

程序会弹出视频窗口循环播放：

```
ROI Preview: press SPACE to freeze and select boxes.
```

1. 让视频播放到你能清楚看到 **R 标志** 和 **扇叶** 的画面
2. 按 **空格键 (SPACE)** 暂停，窗口会冻结在当前帧
3. 会弹出两个选框窗口，依次操作：
   - **Select R**：用鼠标拖动框选 R 中心标志，按 **空格** 或 **回车** 确认
   - **Select Fan Blade**：用鼠标拖动框选扇叶，按 **空格** 或 **回车** 确认

### 第 4 步：自动检测和预测

选完两个框后，程序进入自动运行状态，会弹出主窗口显示：
- 每帧自动检测 R 标志和扇叶
- 画出检测到的目标位置
- 画出预测的下一个打击点（绿色圆圈 + "now predict" 标签）

---

## 弹出的窗口说明

程序运行时你会看到以下几个窗口：

| 窗口名 | 作用 |
|---|---|
| **HSV_Tuner** | 颜色筛选调参面板（见下方说明） |
| **ROI Preview** | 预览视频，找到好帧后按 SPACE 冻结 |
| **Select R** | 鼠标框选 R 中心标志的位置 |
| **Select Fan Blade** | 鼠标框选扇叶的位置 |
| **Original** | 主窗口：实时检测结果 + 预测可视化 |

---

## 播放快捷键

在视频播放期间（ROI 预览和主窗口中都有效）：

| 按键 | 功能 |
|---|---|
| `[` 或 `-` | 减速（每次降 20%） |
| `]` 或 `+` | 加速（每次升 25%） |
| `0` | 重置为 1.0x 正常速度 |
| `SPACE` | 暂停 / 继续播放 |
| `q` 或 `ESC` | 退出程序 |

---

## HSV_Tuner 调参说明

运行时左上角会弹出一个 `HSV_Tuner` 窗口，包含 7 个滑块：

这些参数用来**把视频中你感兴趣的颜色（红/蓝）从背景中分离出来**。

| 滑块 | 含义 | 建议 |
|---|---|---|
| **H Min / H Max** | 色相范围（0-180） | 红色：4~30 或 160~180<br>蓝色：100~130 |
| **S Min / S Max** | 饱和度范围（0-255） | 一般设 50~255，排除灰色背景 |
| **V Min / V Max** | 亮度范围（0-255） | 一般设 80~255，排除暗色区域 |
| **Morph_K** | 形态学核大小（0-20） | 默认 8，用来去除噪点，值越大去噪越强但也会丢失细节 |

**调整技巧**：
- 如果检测不到目标 → 放宽 HSV 范围（增大 H Max、降低 S Min 等）
- 如果检测到太多杂讯 → 缩小 HSV 范围，或增大 Morph_K
- 可以边调边看主窗口中检测框的变化，直到稳定锁定目标

**重要提示**：HSV 参数调节比较复杂，建议先使用专门的调参工具：
```bash
python3 tuner.py /home/zhongxiangyu/BUFF_Tracker/能量机关视频.mp4
```
该工具会同时显示原始视频和二值化效果，调节完成后参数会保存到 `hsv_params.json` 文件中，可以参考其中的数值来调整主程序的参数。

---

## 预测模式说明

代码中有两种预测模式（在 `src/main.cpp` 顶部修改）：

```cpp
const string kPredictColor = "blue";   // "red" 红方 / "blue" 蓝方
const MoveMode kPredictMoveMode = MoveMode::small;  // small 小符 / big 大符
const double kPredictFreq = 50.0;      // 预测频率（Hz），需和视频 FPS 一致
```

- **小符模式**：通过差分+平均值预测，适合高速旋转
- **大符模式**：通过曲线拟合预测，适合匀速旋转
- **红蓝决定旋转方向**：红方顺时针，蓝方逆时针

修改后需要重新编译才能生效。

---

## 常见问题

### 没有 OpenCV？

运行时如果提示找不到 OpenCV，确保系统已安装 `libopencv-dev`：

```bash
sudo apt install libopencv-dev
```

### 视频加载失败？

确认视频文件路径正确，支持绝对路径。也可以在启动时手动输入路径。

### 检测不到扇叶？

1. 先确认 ROI 选的是正确的物体（R 标志和扇叶各选一个）
2. 调整 HSV 参数（参考上方说明）
3. 确保选框时捕捉到了目标清晰的帧（不要太暗或太亮）

### 性能信息显示 "BOTTLENECK"？

说明检测算法太慢，处理时间超过了帧间隔。可以在代码中调整参数或启用 Release 模式编译。

---

## 项目目录结构

```
/home/zhongxiangyu/BUFF_Tracker/
├── build/                    # 编译产物目录
├── include/                  # 头文件目录
│   ├── rune_detector.hpp    # 符文检测器类声明
│   └── angle_predictor.hpp  # 角度预测器类声明
├── src/                      # 源代码目录
│   ├── main.cpp             # 主程序入口
│   ├── rune_detector.cpp    # 核心检测算法实现
│   └── angle_predictor.cpp  # 角度预测算法实现
├── CMakeLists.txt           # CMake构建配置
├── CMakePresets.json        # CMake预设配置
├── tuner.py                 # HSV参数调节工具
├── tuner.md                 # 调参工具说明文档
├── hsv_params.json          # HSV参数保存文件
├── readme.md                # 项目使用指南
└── 能量机关视频.mp4         # 示例视频文件
```

### include/ 头文件

| 文件 | 说明 |
|------|------|
| `include/rune_detector.hpp` | 定义 `RuneDetector` 类、`BBox` 边界框类、`RotationRectangle` 旋转矩形类、`FanBlade` 扇叶类、`RuneParam` 参数结构体等 |
| `include/angle_predictor.hpp` | 定义 `BigPredictor`（大符预测）、`SmallPredictor`（小符预测）、`MovAvg`（移动平均）、`CircularQueue`（循环队列）等预测相关类 |

### src/ 源文件

| 文件 | 说明 |
|------|------|
| `src/main.cpp` | 主程序入口，包含视频加载、用户交互、性能显示等功能 |
| `src/rune_detector.cpp` | 核心检测算法：HSV颜色分割、形态学处理、R定位、扇叶提取、状态机管理 |
| `src/angle_predictor.cpp` | 角度预测算法：小符差分预测、大符拟合预测、角度观察器 |

### 工具与配置

| 文件 | 说明 |
|------|------|
| `tuner.py` | Python版HSV参数实时调节工具，支持保存参数到JSON文件 |
| `tuner.md` | 调参工具详细使用说明文档 |
| `hsv_params.json` | 存储通过tuner.py调节好的HSV参数，供主程序参考 |
| `CMakeLists.txt` | CMake构建脚本，定义编译选项、依赖库、输出目标等 |
| `CMakePresets.json` | 预设的构建配置，如Debug/Release模式 |

---

## 代码阅读指南

### 核心检测流程（必须看）

| 功能 | 文件 | 行号 |
|---|---|---|
| 每帧检测入口 | `src/rune_detector.cpp` | 374 |
| HSV 颜色分割 + 形态学去噪 | `src/rune_detector.cpp` | 192 |
| R 候选筛选与更新 | `src/rune_detector.cpp` | 215, 380 |
| 扇叶提取与状态机 | `src/rune_detector.cpp` | 239, 351 |
| 小符预测（差分 + 平均） | `src/angle_predictor.cpp` | 269 |
| 主程序调用检测 + 预测 | `src/main.cpp` | 369, 381 |

### 可以先忽略的

| 功能 | 文件 | 说明 |
|---|---|---|
| 大符拟合预测 | `src/angle_predictor.cpp` | 大符模式才用到 |
| 预览选框 / 性能显示 | `src/main.cpp` | 交互层代码，非核心算法 |

### 最常调整的参数

| 参数 | 位置 | 作用 |
|---|---|---|
| HSV 阈值 | `src/main.cpp:16` | 颜色检测范围，最常调 |
| 环形掩膜比例 | `include/rune_detector.hpp:76-77` | `outsideRate` / `insideRate`，控制搜索区域大小 |
| 预测频率 | `src/main.cpp:26` | 要和视频真实 FPS 对齐 |
| 红蓝旋转方向 | `src/main.cpp:24, 318` | 决定顺/逆时针预测 |