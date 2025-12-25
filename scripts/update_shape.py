import argparse,subprocess

import os,json,yaml

ros_build = "humble"

def update_topic_shape(topic):
    cmd = "source /opt/ros/{ros_build}/setup.bash && ros2 topic echo {topic} --once --no-arr".format(ros_build=ros_build, topic=topic)
    result = subprocess.run(
        cmd,
        shell=True,
        capture_output=True,
        text=True,
        timeout=5
    )
    result = yaml.safe_load(result.stdout.strip()[:-4])
    shape = [result["width"], result["height"]]
    return shape

def auto_update_shape(args):
    camera_config_path = os.path.join(args.config_dir, 'camera.json')
    with open(camera_config_path, 'r') as f:
        camera_json = json.load(f)
    for i in range(len(camera_json['cameras'])):
        topic = camera_json['cameras'][i]['topic']['image_raw']
        camera_json['cameras'][i]['shape'] = update_topic_shape(topic)
    with open(camera_config_path, 'w') as f:
        json.dump(camera_json, f, indent=4, sort_keys=False)

def parse_args():
    parser = argparse.ArgumentParser(description='Find server ip')
    parser.add_argument('--config_dir', type=str, default='', help='store config path')
    return parser.parse_args()

if __name__ == '__main__':
    args = parse_args()
    auto_update_shape(args)

