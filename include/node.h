#pragma once

#include "ros/ros.h"
#include "cv_bridge/cv_bridge.h"
#include "sensor_msgs/Image.h"
#include "sensor_msgs/image_encodings.h"
#include "image_transport/image_transport.h"

#include "client.hpp"
#include "buffer.hpp"


class RecorderNode{
public:
    RecorderNode(ros::NodeHandle &nh): nh_(nh), it_(nh_) {
        parameter_configuration();
        configuration();
    }
    ~RecorderNode(){
        release();
    }
public:
    void parameter_configuration();
    void configuration();
    void configuration_camera();
    void configuration_client();
    void ImageCallback(const sensor_msgs::ImageConstPtr& msg, int camera_index);
public:
    void start();
    void send_status();
    void compress_video(bool is_record);
    void regular_clean();
    std::string get_string_date(int level);
    void init_video_writer(bool is_record);
    void release();

public:
    void run();

private:
    std::string project_root_;
    std::string launch_file_path_, camera_config_path_, client_config_path_;
    nlohmann::json launch_config_, camera_config_, client_config_;
    std::string save_dir_, save_suffix_;
    bool compress_, show_, debug_;
    int fourcc_, slice_;
    int fps_;
    std::string python_interpeter_;
private:
    std::shared_ptr<RecorderClient> client_;
private:
    std::vector<std::shared_ptr<std::thread>> worker_threads_;
private:
    std::vector<std::string> collect_save_files_;
    std::vector<std::string> record_save_files_;

    std::vector<std::shared_ptr<cv::VideoWriter>> collect_video_writers_;
    std::vector<std::shared_ptr<cv::VideoWriter>> record_video_writers_;

    std::vector<std::atomic<bool>> is_compress_collect_files_;
    std::vector<std::atomic<bool>> is_compress_record_files_;

private:
    ros::NodeHandle nh_;
    image_transport::ImageTransport it_;
    std::vector<image_transport::Subscriber> image_subs_;
    std::vector<std::shared_ptr<TripletBuffer<sensor_msgs::ImageConstPtr>>> buffers_;
private:
    ros::MultiThreadedSpinner spinner_;
    std::thread spin_thread_;
};
