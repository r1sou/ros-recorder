#include "node.h"

void RecorderNode::parse_declare_parameters() {
    this->declare_parameter("project_root", "");
    this->get_parameter("project_root", project_root_);
    RCLCPP_INFO_STREAM(this->get_logger(), "project_root: " << project_root_);

    this->declare_parameter("camera_config_path", "");
    this->get_parameter("camera_config_path", camera_config_path_);
    RCLCPP_INFO_STREAM(this->get_logger(), "camera_config_path: " << camera_config_path_);

    this->declare_parameter("client_config_path", "");
    this->get_parameter("client_config_path", client_config_path_);
    RCLCPP_INFO_STREAM(this->get_logger(), "client_config_path: " << client_config_path_);

    this->declare_parameter("fourcc", 0);
    this->get_parameter("fourcc", fourcc_);
    RCLCPP_INFO_STREAM(this->get_logger(), fmt::format("fourcc is {} | 0: XVID, 1: MJPG, 2: H264, 3: H265",fourcc_).c_str());

    save_suffix_ = fourcc_ < 2 ? ".avi" : ".mp4";
    RCLCPP_INFO_STREAM(this->get_logger(), "save video file suffix: " << save_suffix_);

    if(fourcc_ == 0){
        fourcc_ = cv::VideoWriter::fourcc('X', 'V', 'I', 'D');
    } else if(fourcc_ == 1){
        fourcc_ = cv::VideoWriter::fourcc('M', 'J', 'P', 'G');
    } else if(fourcc_ == 2){
        fourcc_ = cv::VideoWriter::fourcc('a', 'v', 'c', '1');
    } else if(fourcc_ == 3){
        fourcc_ = cv::VideoWriter::fourcc('h', 'v', 'c', '1');
    }

    this->declare_parameter("save_dir", "");
    this->get_parameter("save_dir", save_dir_);
    RCLCPP_INFO_STREAM(this->get_logger(), "video save dir: " << save_dir_);

    this->declare_parameter("slice", 500);
    this->get_parameter("slice", slice_);
    RCLCPP_INFO_STREAM(this->get_logger(), "video slice max size: " << slice_ << " MB");

    this->declare_parameter("fps", 10);
    this->get_parameter("fps", fps_);
    RCLCPP_INFO_STREAM(this->get_logger(), "video fps: " << fps_);

    this->declare_parameter("compress", false);
    this->get_parameter("compress", compress_);
    RCLCPP_INFO_STREAM(this->get_logger(), "video compress: " << compress_);

    this->declare_parameter("show", false);
    this->get_parameter("show", show_);
    RCLCPP_INFO_STREAM(this->get_logger(), "video show: " << show_);

    this->declare_parameter("debug", false);
    this->get_parameter("debug", debug_);
    RCLCPP_INFO_STREAM(this->get_logger(), "video debug: " << debug_);

    this->declare_parameter("python_interpeter", "");
    this->get_parameter("python_interpeter", python_interpeter_);
    RCLCPP_INFO_STREAM(this->get_logger(), "python interpeter: " << python_interpeter_);

    {
        int ret = std::system(fmt::format("{} {}/scripts/find_host.py --config_dir {}/config", python_interpeter_, project_root_, project_root_).c_str());
        if(ret != 0){
            RCLCPP_ERROR_STREAM(this->get_logger(), "find host failed");
        }
        {
            std::ifstream file(client_config_path_);
            file >> client_config_;
            RCLCPP_INFO_STREAM(this->get_logger(), "client config: " << client_config_.dump(4));
        }
    }
    {
        int ret = std::system(fmt::format("{} {}/scripts/update_shape.py --config_dir {}/config", python_interpeter_, project_root_, project_root_).c_str());
        if(ret != 0){
            RCLCPP_ERROR_STREAM(this->get_logger(), "update shape failed");
        }
        {
            std::ifstream file(camera_config_path_);
            file >> camera_config_;
            RCLCPP_INFO_STREAM(this->get_logger(), "camera config: " << camera_config_.dump(4));
        }
    }
}

void RecorderNode::configuration() {
    configuration_camera();
    configuration_client();
}

void RecorderNode::configuration_camera() {
    for(int i = 0; i < camera_config_["cameras"].size(); i++){
        auto &camera_config = camera_config_["cameras"][i];
        auto buffer_ = std::make_shared<TripletBuffer<sensor_msgs::msg::Image::SharedPtr>>();
        auto image_sub_ = create_subscription<sensor_msgs::msg::Image>(
            camera_config["topic"]["image_raw"].get<std::string>(), 10, 
            [this, i](sensor_msgs::msg::Image::SharedPtr msg)
            {
                ImageCallback(msg, i);
            }
        );
        buffers_.push_back(buffer_);
        image_subs_.push_back(image_sub_);
    }
    is_compress_collect_files_ = std::vector<std::atomic<bool>>(camera_config_["cameras"].size());
    is_compress_record_files_ = std::vector<std::atomic<bool>>(camera_config_["cameras"].size());
}

void RecorderNode::configuration_client() {
    std::string uri = fmt::format(
        "ws://{}:{}", 
        client_config_["client"]["ip"].get<std::string>(), 
        client_config_["client"]["port"].get<std::string>()
    );
    client_ = std::make_shared<RecorderClient>(uri);
}

void RecorderNode::ImageCallback(sensor_msgs::msg::Image::SharedPtr msg, int camera_index) {
    buffers_[camera_index]->update(
        [msg](sensor_msgs::msg::Image::SharedPtr &msg_)
        {
            msg_ = msg;
        }
    );
}

void RecorderNode::start() {
    executor_ = std::make_shared<rclcpp::executors::MultiThreadedExecutor>();
    executor_->add_node(this->shared_from_this());
    spin_thread_ = std::thread(
        [this](){
            executor_->spin(); 
        }
    );
    client_->start();
    worker_threads_.emplace_back(
        std::make_shared<std::thread>(
            [this](){
                rclcpp::WallRate rate(1.0 / 2);
                while(rclcpp::ok()){
                    send_status();
                    rate.sleep();
                }
                RCLCPP_INFO_STREAM(this->get_logger(), "send status thread exit");
            }
        )
    );
    worker_threads_.emplace_back(
        std::make_shared<std::thread>(
            [this](){
                rclcpp::WallRate rate(1.0 / 600);
                while(rclcpp::ok()){
                    compress_video(true);
                    compress_video(false);
                    rate.sleep();
                }
                RCLCPP_INFO_STREAM(this->get_logger(), "compress video thread exit");
            }
        )
    );
    worker_threads_.emplace_back(
        std::make_shared<std::thread>(
            [this](){
                rclcpp::WallRate rate(1.0 / 3600);
                while(rclcpp::ok()){
                    regular_clean();
                    rate.sleep();
                }
                RCLCPP_INFO_STREAM(this->get_logger(), "regular clean thread exit");
            }
        )
    );
}

void RecorderNode::send_status() {
    if(!client_->connected.load()){
        return;
    }
    nlohmann::json message;
    message["device_id"] = 1;
    message["cmd_code"] = 0x14;
    time_t timestamp = time(NULL);
    message["time_stamp"] = timestamp;
    message["data"]["cam_status"] = client_->start_collect.load() || client_->start_record.load() ? 1 : 0;
    message["key"] = JWTGenerator::generate(client_config_["jwt"]["req_id"], client_config_["jwt"]["key"]);
    client_->send_message(message.dump());
}

void RecorderNode::compress_video(bool is_record) {
    std::string cmd = "ffmpeg -i {} {}";

    auto &writers = is_record ? record_video_writers_ : collect_video_writers_;
    auto &save_files = is_record ? record_save_files_ : collect_save_files_;

    if(save_files.size() == 0 || writers.size() == 0){
        return;
    }

    for(int i = 0; i < camera_config_["cameras"].size(); i++){
        if(!std::filesystem::exists(save_files[i])){
            continue;
        }
        float file_size = std::filesystem::file_size(save_files[i]) / 1024.0 / 1024.0;
        if(file_size > slice_){
            if(is_record){
                is_compress_record_files_[i].store(true);
            } else {
                is_compress_collect_files_[i].store(true);
            }
            for(int t = 0; t < 3 && rclcpp::ok(); t++){
                std::this_thread::sleep_for(std::chrono::seconds(1));
            }
            {
                writers[i]->release();
            }
            {
                std::string save_dirs = save_dir_ + "/" + get_string_date(0) + "/" + camera_config_["cameras"][i]["name"].get<std::string>(); 
                std::string save_path = save_dirs + "/" + get_string_date(2);
                save_path += is_record ? "-record" : "-collect";
                save_path += save_suffix_;
                std::vector<int> shape = camera_config_["cameras"][i]["shape"];

                writers[i] = std::make_shared<cv::VideoWriter>(
                    save_path, fourcc_, fps_, cv::Size(shape[0], shape[1])
                );
                save_files[i] = save_path;
            }
        }
    }
}

void RecorderNode::regular_clean() {
    if(!std::filesystem::exists(save_dir_)){
        return;
    }
    std::string cmd = fmt::format("{} {}/scripts/regular_clean.py --dataset {}", python_interpeter_, project_root_, save_dir_);
    int ret = std::system(cmd.c_str());
    if(ret != 0){
        RCLCPP_ERROR_STREAM(this->get_logger(), "\33[31mregular file cleanup failed\33[0m");
    }
}

std::string RecorderNode::get_string_date(int level) {
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

void RecorderNode::init_video_writer(bool is_record){
    auto &writers = is_record ? record_video_writers_ : collect_video_writers_;
    auto &save_files = is_record ? record_save_files_ : collect_save_files_;

    for(auto &camera_config: camera_config_["cameras"]){
        std::string save_dirs = save_dir_ + "/" + get_string_date(0) + "/" + camera_config["name"].get<std::string>();
        std::string save_path = save_dirs + "/" + get_string_date(2);
        std::filesystem::create_directories(save_dirs);
        save_path += is_record ? "-record" : "-collect";
        save_path += save_suffix_;

        std::vector<int> shape = camera_config["shape"];

        writers.push_back(std::make_shared<cv::VideoWriter>(
            save_path, fourcc_, fps_, cv::Size(shape[0], shape[1])
        ));
        save_files.push_back(save_path);
    }
}

void RecorderNode::release() {
    for(auto &t : worker_threads_){
        if(t->joinable()){
            t->join();
        }
    }
    worker_threads_.clear();

    for(auto &writer_ : record_video_writers_){
        writer_->release();
    }
    record_video_writers_.clear();
    for(auto &writer_ : collect_video_writers_){
        writer_->release();
    }
    collect_video_writers_.clear();

    if(spin_thread_.joinable()){
        spin_thread_.join();
    }
}

void RecorderNode::run() {
    bool is_running_record = client_->start_record.load() || debug_;
    bool is_running_collect = client_->start_collect.load() || debug_;
    
    if(!is_running_record){
        for(auto &writer_: record_video_writers_){
            writer_->release();
        }
        record_video_writers_.clear();
        record_save_files_.clear();
    }
    if(!is_running_collect){
        for(auto &writer_: collect_video_writers_){
            writer_->release();
        }
        collect_video_writers_.clear();
        collect_save_files_.clear();
    }
    if(!is_running_record && !is_running_collect){
        return;
    }

    if(is_running_record && record_video_writers_.size() == 0){
        init_video_writer(true);
    }
    if(is_running_collect && collect_video_writers_.size() == 0){
        init_video_writer(false);
    }

    for(int i = 0; i < camera_config_["cameras"].size(); i++){
        auto element = buffers_[i]->read();
        if (!element || !element->msg){
            continue;
        }

        cv::Mat image;
        std::string encoding = element->msg->encoding;
        if(encoding == "bgr8" || encoding == "BGR8" || encoding == "rgb8" || encoding == "RGB8"){
            image = cv_bridge::toCvShare(element->msg, element->msg->encoding)->image;
        }
        else if(encoding == "nv12" || encoding == "NV12"){
            ScopeTimer scope("nv12_to_bgr");
#ifdef __aarch64__
            image = cv::Mat(element->msg->height, element->msg->width, CV_8UC3);
            image_conversion::nv12_to_bgr24_neon(element->msg->data.data(), image.data, element->msg->width, element->msg->height);
#else
            image = cv::Mat(element->msg->height * 3 / 2, element->msg->width, CV_8UC1, element->msg->data.data(), element->msg->step);
            cv::cvtColor(image, image, cv::COLOR_YUV2BGR_NV12);
#endif
        }

        {
            int64_t img_pub_time = element->msg->header.stamp.sec * 1000LL + element->msg->header.stamp.nanosec / 1000000;
            int64_t img_store_time = element->timestamp.count();
            std::string info = fmt::format("img pub time: {:.3f}, img store time: {:.3f}", img_pub_time / 1000.0, img_store_time / 1000.0);
            RCLCPP_INFO_STREAM(this->get_logger(), info.c_str());
        }

        if(show_){
            cv::imshow(fmt::format("camera {}", i), image);
            cv::waitKey(1);
        }
        if(is_running_record && !is_compress_record_files_[i].load()){
            record_video_writers_[i]->write(image);
        }
        if(is_running_collect && !is_compress_collect_files_[i].load()){
            collect_video_writers_[i]->write(image);
        }
    }
}