#pragma once

#include "rclcpp/rclcpp.hpp"
#include "cv_bridge/cv_bridge.h"
#include "sensor_msgs/msg/image.hpp"

#include "common/buffer.hpp"
#include "common/image_conversion.hpp"

class RecorderNode: public rclcpp::Node{
public:
    RecorderNode(const rclcpp::NodeOptions &node_options = rclcpp::NodeOptions()): Node("recorder_node", node_options){
        parse_declare_parameters();
        configuration();
    }
    ~RecorderNode(){
        stop();
    }
public:
    void parse_declare_parameters(){
        this->project_root_ = this->declare_parameter<std::string>("project_root", "");
        RCLCPP_INFO_STREAM(this->get_logger(), "project_root: " << this->project_root_);

        this->save_dir_ = this->declare_parameter<std::string>("save_dir", "");
        RCLCPP_INFO_STREAM(this->get_logger(), "save_dir: " << this->save_dir_);

        this->collect_ = this->declare_parameter<bool>("collect", false);
        RCLCPP_INFO_STREAM(this->get_logger(), "collect: " << this->collect_);

        this->record_ = this->declare_parameter<bool>("record", false);
        RCLCPP_INFO_STREAM(this->get_logger(), "record: " << this->record_);

        this->fourcc_ = this->declare_parameter<int>("fourcc", 0);
        RCLCPP_INFO_STREAM(this->get_logger(), "fourcc: " << this->fourcc_ << " | 0: XVID, 1: MJPG, 2: H264, 3: H265");

        this->suffix_ = this->declare_parameter<std::string>("suffix", "avi");
        RCLCPP_INFO_STREAM(this->get_logger(), "suffix: " << this->suffix_);
        
        this->fps_ = this->declare_parameter<int>("fps", 10);
        RCLCPP_INFO_STREAM(this->get_logger(), "fps: " << this->fps_);

        this->show_ = this->declare_parameter<bool>("show", false);
        RCLCPP_INFO_STREAM(this->get_logger(), "show: " << this->show_);
        
        this->debug_ = this->declare_parameter<bool>("debug", false);
        RCLCPP_INFO_STREAM(this->get_logger(), "debug: " << this->debug_);

        {
            std::filesystem::create_directories(save_dir_);
            if(fourcc_ == 0){
                fourcc_ = cv::VideoWriter::fourcc('X', 'V', 'I', 'D');
            } else if(fourcc_ == 1){
                fourcc_ = cv::VideoWriter::fourcc('M', 'J', 'P', 'G');
            } else if(fourcc_ == 2){
                fourcc_ = cv::VideoWriter::fourcc('a', 'v', 'c', '1');
            } else if(fourcc_ == 3){
                fourcc_ = cv::VideoWriter::fourcc('h', 'v', 'c', '1');
            }
        }
    }
    void configuration(){
        configuration_camera();
    }
    void configuration_camera(){
        buffer_ = std::make_shared<TripletBuffer<sensor_msgs::msg::Image::SharedPtr>>();
        image_sub_ = create_subscription<sensor_msgs::msg::Image>(
            "/image_left_raw", 10, 
            [this](sensor_msgs::msg::Image::SharedPtr msg)
            {
                ImageCallback(msg);
            }
        );
    }
    void ImageCallback(sensor_msgs::msg::Image::SharedPtr msg){
        buffer_->update(
            [msg](sensor_msgs::msg::Image::SharedPtr &msg_)
            {
                msg_ = msg;
            }
        );
    }
public:
    void start(){
        executor_ = std::make_shared<rclcpp::executors::MultiThreadedExecutor>();
        executor_->add_node(this->shared_from_this());
        spin_thread_ = std::thread(
            [this](){
                executor_->spin(); 
            }
        );
    }
    void stop(){
        for(auto &t : worker_threads_){
            if(t->joinable()){
                t->join();
            }
        }
        worker_threads_.clear();
        RCLCPP_INFO_STREAM(this->get_logger(), "worker threads joined");
        {
            if(collect_writers_){
                collect_writers_->release();
            }
            if(record_writers_){
                record_writers_->release();
            }
        }
        RCLCPP_INFO_STREAM(this->get_logger(), "video writers released");
        if(spin_thread_.joinable()){
            spin_thread_.join();
        }
        RCLCPP_INFO_STREAM(this->get_logger(), "spin thread joined");
    }
public:
    std::string get_string_date(int level) {
        time_t timestamp = time(NULL);

        struct tm *tm_time = localtime(&timestamp);

        std::ostringstream oss;
        if(level == 0){
            oss << std::put_time(tm_time, "%Y-%m-%d");
        }
        else if(level == 1){
            oss << std::put_time(tm_time, "%m-%d");
        }
        else if(level == 2){
            oss << std::put_time(tm_time, "%H-%M");
        }
        return oss.str();
    }
    void run(){
        static int image_H = 352, image_W = 640;

        while(true){
            auto element = buffer_->read();
            if (!element || !element->msg){
                std::this_thread::sleep_for(std::chrono::milliseconds(100));
                continue;
            }
            image_H = element->msg->height;
            image_W = element->msg->width;
            break;
        }

        if((collect_ || debug_) && !collect_writers_){
            std::string filename = save_dir_ + "/" + get_string_date(2) + "-collect." + suffix_;
            collect_writers_ = std::make_shared<cv::VideoWriter>(filename, fourcc_, fps_, cv::Size(image_W, image_H));
        }
        if((record_ || debug_) && !record_writers_){
            std::string filename = save_dir_ + "/" + get_string_date(2) + "-record." + suffix_;
            record_writers_ = std::make_shared<cv::VideoWriter>(filename, fourcc_, fps_, cv::Size(image_W, image_H));
        }

        // rclcpp::WallRate rate(1.0 / fps_);
        while(rclcpp::ok()){
            auto element = buffer_->read();
            if (!element || !element->msg){
                std::this_thread::sleep_for(std::chrono::milliseconds(100));
                continue;
            }
            cv::Mat image;
            std::string encoding = element->msg->encoding;
            if(encoding == "bgr8" || encoding == "BGR8" || encoding == "rgb8" || encoding == "RGB8"){
                image = cv_bridge::toCvShare(element->msg, element->msg->encoding)->image;
            }
            else if(encoding == "nv12" || encoding == "NV12"){
                cv::Mat tmp = cv::Mat(element->msg->height * 3 / 2, element->msg->width, CV_8UC1, element->msg->data.data(), element->msg->step);
                image_conversion::nv12_to_bgr(tmp, image);
            }
            if(collect_ || debug_){
                collect_writers_->write(image);
            }
            if(record_ || debug_){
                record_writers_->write(image);
            }
            if(show_){
                cv::imshow("image", image);
                cv::waitKey(1);
            }
            // rate.sleep();
        }
    }
private:
    std::string project_root_, save_dir_;
    int fourcc_; 
    std::string suffix_;
    int fps_;
    bool collect_, record_, show_, debug_;
private:
    std::vector<std::shared_ptr<std::thread>> worker_threads_;
private:
    std::shared_ptr<cv::VideoWriter> collect_writers_, record_writers_;
private:
    rclcpp::Subscription<sensor_msgs::msg::Image>::SharedPtr image_sub_;
    std::shared_ptr<TripletBuffer<sensor_msgs::msg::Image::SharedPtr>> buffer_;
private:
    std::shared_ptr<rclcpp::executors::MultiThreadedExecutor> executor_;
    std::thread spin_thread_;
};