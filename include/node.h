#pragma once

#include "rclcpp/rclcpp.hpp"
#include "rclcpp/rclcpp.hpp"
#include "cv_bridge/cv_bridge.h"
#include "sensor_msgs/msg/image.hpp"

#include "client.hpp"
#include "buffer.hpp"
#include "image_conversion.hpp"

class ScopeTimer{
public:
    ScopeTimer(std::string name): name(name){
        start = std::chrono::system_clock::now();
    }
    ~ScopeTimer(){
        auto end = std::chrono::system_clock::now();
        const std::chrono::duration<float, std::milli> duration = end - start;
        std::string info = fmt::format("\033[32mScope {} end, duration: {:.3f}ms\033[0m", name, duration.count());
        RCLCPP_INFO_STREAM(rclcpp::get_logger("ScopeTimer"), info.c_str());
    }
public:
    std::string name;
    std::chrono::system_clock::time_point start;
};

class RecorderNode: public rclcpp::Node{
public:
    RecorderNode(const rclcpp::NodeOptions &node_options = rclcpp::NodeOptions()) : Node("collect_node", node_options){
        parse_declare_parameters();
        configuration();
    }
    ~RecorderNode(){
        release();
    }
public:
    void parse_declare_parameters();
    void configuration();
    void configuration_camera();
    void configuration_client();
    void ImageCallback(sensor_msgs::msg::Image::SharedPtr msg, int camera_index);
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
    std::string camera_config_path_, client_config_path_;
    nlohmann::json camera_config_, client_config_;
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
    std::vector<std::shared_ptr<TripletBuffer<sensor_msgs::msg::Image::SharedPtr>>> buffers_;
    std::vector<rclcpp::Subscription<sensor_msgs::msg::Image>::SharedPtr> image_subs_;
private:
    std::shared_ptr<rclcpp::executors::MultiThreadedExecutor> executor_;
    std::thread spin_thread_;
};
