from setuptools import find_packages, setup

package_name = 'color_detection'

setup(
    name=package_name,
    version='0.0.0',
    packages=find_packages(exclude=['test']),
    data_files=[
        ('share/ament_index/resource_index/packages',
            ['resource/' + package_name]),
        ('share/' + package_name, ['package.xml']),
    ],
    install_requires=['setuptools'],
    zip_safe=True,
    maintainer='zhongxiangyu',
    maintainer_email='zhongxiangyu@todo.todo',
    description='TODO: Package description',
    license='TODO: License declaration',
    extras_require={
        'test': [
            'pytest',
        ],
    },
    entry_points={#节点入口点，用于在ROS2中注册节点，需要在setup.py中配置
    'console_scripts': [
        'camera_publisher = color_detection.camera_publisher:main',
        'color_detector = color_detection.color_detector:main',
    ],
},
)
