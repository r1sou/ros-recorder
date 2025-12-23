#include "node.h"

int main(int argc, char **argv)
{
    ros::init(argc, argv, "recorder_node");
    ros::NodeHandle nh;
    
    auto node = std::make_shared<RecorderNode>(nh);
    node->start();    

    ros::Rate loop_rate(10);
    while (ros::ok())
    {
        node->run();   
        loop_rate.sleep();
    }

    ros::shutdown();
    cv::destroyAllWindows();
    return 0;
}