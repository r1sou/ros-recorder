from math import inf
from launch import LaunchDescription
from launch.actions import DeclareLaunchArgument
from launch.substitutions import LaunchConfiguration
from launch_ros.actions import Node

from ament_index_python.packages import get_package_share_directory

import os,json,yaml,subprocess,platform,shutil

from datetime import datetime

def declare_configurable_parameters(parameters):
    return [
        DeclareLaunchArgument(param["name"], default_value=param["default_value"])
        for param in parameters
    ]
    
def set_configurable_parameters(parameters):
    return dict([(param['name'], LaunchConfiguration(param['name'])) for param in parameters])


def generate_launch_description():

    root = get_package_share_directory('recorder')

    node_params = [
        {"name": "node_name", "default_value": "recorder_node"},
        {"name": "log_level", "default_value": "info"},

        {"name": "project_root", "default_value": root},
        {"name": "save_dir", "default_value": "/home/sunrise/Desktop/dataset"},

        {"name": "collect", "default_value": "False"},
        {"name": "record", "default_value": "False"},

        {"name": "fourcc", "default_value": "0"},
        {"name": "fps", "default_value": "10"},

        {"name": "show", "default_value": "False"},
        {"name": "debug", "default_value": "False"},
    ]

    launch = declare_configurable_parameters(node_params)

    launch.append(Node(
        package='recorder',
        executable='recorder_node',
        name=LaunchConfiguration('node_name'),
        output='screen',
        parameters=[set_configurable_parameters(node_params)],
        arguments=['--ros-args', '--log-level', LaunchConfiguration('log_level')]
    ))

    return LaunchDescription(launch)