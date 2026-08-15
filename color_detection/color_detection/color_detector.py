import rclpy
from rclpy.node import Node
from sensor_msgs.msg import Image
from cv_bridge import CvBridge
import cv2
import numpy as np


class ColorDetector(Node):
    def __init__(self):
        super().__init__('color_detector')
        self.subscription = self.create_subscription(Image, 'camera/image_raw', self.image_callback, 10)
        self.bridge = CvBridge()
        self.get_logger().info('ColorDetector node started, waiting for images...')

    def draw_color_boxes(self, cv_img, mask, color_name, box_color):
        kernel = np.ones((3, 3), np.uint8)
        mask = cv2.morphologyEx(mask, cv2.MORPH_CLOSE, kernel)  # 闭运算：填充空洞
        mask = cv2.morphologyEx(mask, cv2.MORPH_OPEN, kernel)   # 开运算：去除噪点
        contours, _ = cv2.findContours(mask, cv2.RETR_EXTERNAL, cv2.CHAIN_APPROX_SIMPLE)
        for contour in contours:
            area = cv2.contourArea(contour)
            if area < 100:  # 过滤小面积噪点
                continue
            x, y, w, h = cv2.boundingRect(contour)
            cv2.rectangle(cv_img, (x, y), (x + w, y + h), box_color, 3)  # 绘制矩形框
            label = f'{color_name} {w}x{h}'
            cv2.putText(cv_img, label, (x, y - 10), cv2.FONT_HERSHEY_SIMPLEX, 0.7, box_color, 2)  # 显示标签
        return mask

    def image_callback(self, msg):
        cv_img = self.bridge.imgmsg_to_cv2(msg, 'bgr8')
        display_img = cv_img.copy()
        hsv = cv2.cvtColor(cv_img, cv2.COLOR_BGR2HSV)

        # 蓝色检测（HSV空间）
        lower_blue = np.array([100, 150, 50])
        upper_blue = np.array([140, 255, 255])
        blue_mask = cv2.inRange(hsv, lower_blue, upper_blue)
        blue_mask_processed = self.draw_color_boxes(display_img, blue_mask, 'Blue', (0, 255, 0))

        # 红色检测（BGR空间）
        lower_red_bgr = np.array([0, 0, 150])
        upper_red_bgr = np.array([100, 100, 255])
        red_mask = cv2.inRange(cv_img, lower_red_bgr, upper_red_bgr)
        red_mask_processed = self.draw_color_boxes(display_img, red_mask, 'Red', (0, 255, 0))

        # 显示结果
        cv2.imshow('Color Detection with Bounding Boxes', display_img)
        cv2.imshow('Blue Binary Mask', blue_mask_processed)
        cv2.imshow('Red Binary Mask', red_mask_processed)
        cv2.waitKey(1)


def main(args=None):
    rclpy.init(args=args)
    color_detector = ColorDetector()
    try:
        rclpy.spin(color_detector)
    except KeyboardInterrupt:
        pass
    finally:
        color_detector.destroy_node()
        rclpy.shutdown()
        cv2.destroyAllWindows()


if __name__ == '__main__':
    main()
