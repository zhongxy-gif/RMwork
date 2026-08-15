import rclpy
from rclpy.node import Node
from sensor_msgs.msg import Image
from cv_bridge import CvBridge
import cv2


class CameraPublisher(Node):
    def __init__(self):
        super().__init__('camera_publisher')
        self.publisher_ = self.create_publisher(Image, 'camera/image_raw', 10)  # 创建图像发布者
        self.bridge = CvBridge()
        self.cap = cv2.VideoCapture(0)  # 打开摄像头
        self.timer = self.create_timer(0.1, self.timer_callback)  # 10Hz定时发布

    def timer_callback(self):
        ret, frame = self.cap.read()
        if ret:
            img_msg = self.bridge.cv2_to_imgmsg(frame, 'bgr8')  # OpenCV转ROS2消息
            self.publisher_.publish(img_msg)

    def destroy(self):
        self.cap.release()  # 释放摄像头
        super().destroy()


def main(args=None):
    rclpy.init(args=args)
    camera_publisher = CameraPublisher()
    rclpy.spin(camera_publisher)
    camera_publisher.destroy_node()
    rclpy.shutdown()


if __name__ == '__main__':
    main()
