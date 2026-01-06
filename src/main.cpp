#include "node/node.h"

int main(int argc, char **argv){
    rclcpp::init(argc, argv);

    auto node = std::make_shared<RecorderNode>();
    node->start();

    rclcpp::WallRate loop_rate(10);

    node->run();

    rclcpp::shutdown();
    cv::destroyAllWindows();
    return 0;
}