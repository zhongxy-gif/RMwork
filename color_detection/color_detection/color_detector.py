import rclpy  # ROS2 Python 客户端库
from rclpy.node import Node  # ROS2 节点基类
from sensor_msgs.msg import Image  # 图像消息类型
from cv_bridge import CvBridge  # ROS2 与 OpenCV 图像转换桥接
import cv2  # OpenCV 图像处理库
import numpy as np  # 数值计算库


class ColorDetector(Node):  # 颜色检测节点
    def __init__(self):
        super().__init__('color_detector')  # 节点名
        self.subscription = self.create_subscription(Image, 'camera/image_raw', self.image_callback, 10)  # 创建订阅者
        self.bridge = CvBridge()  # 图像转换器
        self.get_logger().info('ColorDetector node started, waiting for images...')  # 启动日志

    def draw_color_boxes(self, cv_img, mask, color_name, box_color):
        # 形态学核：3x3矩阵（更小，避免过度平滑），用于去噪
        kernel = np.ones((3, 3), np.uint8)
        # 闭运算：填充mask内部的小空洞，连接断裂区域
        mask = cv2.morphologyEx(mask, cv2.MORPH_CLOSE, kernel)
        # 开运算：去除mask边缘的小噪点
        mask = cv2.morphologyEx(mask, cv2.MORPH_OPEN, kernel)
        # 查找所有外部轮廓
        contours, _ = cv2.findContours(mask, cv2.RETR_EXTERNAL, cv2.CHAIN_APPROX_SIMPLE)
        # 遍历每个轮廓，绘制识别框
        for contour in contours:
            # 计算轮廓面积
            area = cv2.contourArea(contour)
            # 过滤小面积噪点（阈值降为100，更容易检测小物体）
            if area < 100:
                continue
            # 获取轮廓的外接矩形坐标 (x, y)为左上角，(w, h)为宽高
            x, y, w, h = cv2.boundingRect(contour)
            # 在图像上绘制矩形识别框，线宽为3（更醒目）
            cv2.rectangle(cv_img, (x, y), (x + w, y + h), box_color, 3)
            # 构造标签文本：颜色名 + 宽高尺寸
            label = f'{color_name} {w}x{h}'
            # 在框上方绘制标签文字，字号0.7，线宽2
            cv2.putText(cv_img, label, (x, y - 10),
                        cv2.FONT_HERSHEY_SIMPLEX, 0.7, box_color, 2)
        return mask  # 返回处理后的二值掩膜（白色为检测区域，黑色为背景）

    def image_callback(self, msg):  # 图像接收回调
        # 将ROS图像消息转换为OpenCV图像（BGR格式）
        cv_img = self.bridge.imgmsg_to_cv2(msg, 'bgr8')

        # 复制原图用于绘制，避免修改原始数据
        display_img = cv_img.copy()
        # 转换为HSV颜色空间（用于蓝色检测）
        hsv = cv2.cvtColor(cv_img, cv2.COLOR_BGR2HSV)

        # ---------- 蓝色检测（HSV 空间） ----------
        lower_blue = np.array([100, 150, 50])   # H,S,V下界
        upper_blue = np.array([140, 255, 255])  # H,S,V上界
        blue_mask = cv2.inRange(hsv, lower_blue, upper_blue)  # 生成蓝色掩膜（二值图）
        blue_mask_processed = self.draw_color_boxes(display_img, blue_mask, 'Blue', (0, 255, 0))  # 绘制蓝色框，返回处理后掩膜

        # ---------- 红色检测（BGR 空间，用户要求保持） ----------
        lower_red_bgr = np.array([0, 0, 150])    # B=0, G=0, R=150
        upper_red_bgr = np.array([100, 100, 255]) # B=100, G=100, R=255
        red_mask = cv2.inRange(cv_img, lower_red_bgr, upper_red_bgr)  # 生成红色掩膜（二值图）
        red_mask_processed = self.draw_color_boxes(display_img, red_mask, 'Red', (0, 255, 0))  # 绘制红色框，返回处理后掩膜

        # 调试：打印掩膜非零像素数（用于确认检测效果）
        blue_pixels = cv2.countNonZero(blue_mask_processed)
        red_pixels = cv2.countNonZero(red_mask_processed)
        self.get_logger().info(f'Blue pixels: {blue_pixels}, Red pixels: {red_pixels}')

        # 显示三个窗口：
        # 1. 彩色图像 + 检测框（绿色框）
        cv2.imshow('Color Detection with Bounding Boxes', display_img)
        # 2. 蓝色二值掩膜（白色=蓝色区域，黑色=其他）
        cv2.imshow('Blue Binary Mask', blue_mask_processed)
        # 3. 红色二值掩膜（白色=红色区域，黑色=其他）
        cv2.imshow('Red Binary Mask', red_mask_processed)
        cv2.waitKey(1)  # 必须调用，用于刷新窗口


def main(args=None):
    rclpy.init(args=args)  # 初始化 ROS2
    color_detector = ColorDetector()  # 创建节点
    try:
        rclpy.spin(color_detector)  # 进入事件循环（阻塞）
    except KeyboardInterrupt:
        pass  # 允许Ctrl+C退出
    finally:
        color_detector.destroy_node()  # 销毁节点
        rclpy.shutdown()  # 关闭 ROS2
        cv2.destroyAllWindows()  # 关闭所有 OpenCV 窗口


if __name__ == '__main__':
    main()