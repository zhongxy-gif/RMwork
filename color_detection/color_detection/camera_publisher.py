import rclpy  # ROS2 Python 客户端库
from rclpy.node import Node  # ROS2 节点基类
from sensor_msgs.msg import Image  # 图像消息类型
from cv_bridge import CvBridge  # ROS2 与 OpenCV 图像转换桥接
import cv2  # OpenCV 图像处理库


class CameraPublisher(Node):  # 摄像头发布者节点
    def __init__(self):
        super().__init__('camera_publisher')  # 节点名
        self.publisher_ = self.create_publisher(Image, 'camera/image_raw', 10)  # 创建发布者
        self.bridge = CvBridge()  # 图像转换器
        self.cap = cv2.VideoCapture(0)  # 打开默认摄像头
        self.timer = self.create_timer(0.1, self.timer_callback)  # 10Hz 定时器

    def timer_callback(self):  # 定时器回调：采集并发布图像
        ret, frame = self.cap.read()  # 读取一帧
        if ret:
            img_msg = self.bridge.cv2_to_imgmsg(frame, 'bgr8')  # 转成 ROS2 消息
            self.publisher_.publish(img_msg)  # 发布到话题
            self.get_logger().info('Publishing camera image')  # 打印日志

    def destroy(self):  # 析构：释放资源
        self.cap.release()  # 释放摄像头
        super().destroy()


def main(args=None):
    rclpy.init(args=args)  # 初始化 ROS2
    camera_publisher = CameraPublisher()  # 创建节点
    rclpy.spin(camera_publisher)  # 进入事件循环
    camera_publisher.destroy_node()  # 销毁节点
    rclpy.shutdown()  # 关闭 ROS2


if __name__ == '__main__':
    main()