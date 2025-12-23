#include "node.h"

void RecorderNode::parameter_configuration() {
    {
        launch_file_path_ = LAUNCH_FILE_PATH;
        std::ifstream file(launch_file_path_);
        file >> launch_config_;
    }
    // parse params
    {
        project_root_ = PROJECT_ROOT;
        ROS_INFO_STREAM("project root: " << project_root_);
        ROS_INFO_STREAM("launch config file: " << launch_file_path_);
        ROS_INFO_STREAM("camera config file: " << camera_config_path_);
        ROS_INFO_STREAM("client config file: " << client_config_path_);

        fourcc_ = launch_config_["fourcc"].get<int>();
        ROS_INFO_STREAM(fmt::format("fourcc is {} | 0: XVID, 1: MJPG, 2: H264, 3: H265", fourcc_).c_str());

        save_suffix_ = fourcc_ < 2 ? ".avi" : ".mp4";
        ROS_INFO_STREAM("save video file suffix: " << save_suffix_);

        if(fourcc_ == 0){
            fourcc_ = cv::VideoWriter::fourcc('X', 'V', 'I', 'D');
        } else if(fourcc_ == 1){
            fourcc_ = cv::VideoWriter::fourcc('M', 'J', 'P', 'G');
        } else if(fourcc_ == 2){
            fourcc_ = cv::VideoWriter::fourcc('a', 'v', 'c', '1');
        } else if(fourcc_ == 3){
            fourcc_ = cv::VideoWriter::fourcc('h', 'v', 'c', '1');
        }

        save_dir_ = launch_config_["save_dir"].get<std::string>();
        ROS_INFO_STREAM("save video file dir: " << save_dir_);

        slice_ = launch_config_["slice"].get<int>();
        ROS_INFO_STREAM("video slice max size: " << slice_ << " MB");

        compress_ = launch_config_["compress"].get<bool>();
        show_ = launch_config_["show"].get<bool>();
        debug_ = launch_config_["debug"].get<bool>();
        fps_ = launch_config_["fps"].get<int>();
        
        python_interpeter_ = launch_config_["python"].get<std::string>();

        ROS_INFO_STREAM("do compress: " << compress_);
        ROS_INFO_STREAM("do show: " << show_);
        ROS_INFO_STREAM("do debug: " << debug_);
        ROS_INFO_STREAM("fps: " << fps_);

        ROS_INFO_STREAM("python interpeter: " << python_interpeter_);
    }
    {
        int ret = std::system(fmt::format("{} {}/scripts/find_host.py --config_dir {}/assets/config",python_interpeter_, project_root_, project_root_).c_str());
        if(ret != 0){
            ROS_ERROR_STREAM("find host failed");
        }
        {
            camera_config_path_ = CAMERA_FILE_PATH;
            std::ifstream file(camera_config_path_);
            file >> camera_config_;
        }
        {
            client_config_path_ = CLIENT_FILE_PATH;
            std::ifstream file(client_config_path_);
            file >> client_config_;
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
        auto buffer_ = std::make_shared<TripletBuffer<sensor_msgs::ImageConstPtr>>();
        auto image_sub_ = it_.subscribe(
            camera_config["topic"]["image_raw"].get<std::string>(), 10, 
            [this, i](const sensor_msgs::ImageConstPtr& msg)
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

void RecorderNode::ImageCallback(const sensor_msgs::ImageConstPtr& msg, int camera_index) {
    buffers_[camera_index]->update(
        [msg](sensor_msgs::ImageConstPtr &msg_)
        {
            msg_ = msg;
        }
    );
}

void RecorderNode::start() {
    spin_thread_ = std::thread(
        [this](){
            spinner_.spin();
        }
    );
    client_->start();
    worker_threads_.emplace_back(
        std::make_shared<std::thread>(
            [this](){
                ros::Rate rate(1.0 / 2);
                while(ros::ok()){
                    send_status();
                    rate.sleep();
                }
                std::cout << "send status thread exit" << std::endl;
            }
        )
    );
    worker_threads_.emplace_back(
        std::make_shared<std::thread>(
            [this](){
                while(ros::ok()){
                    compress_video(true);
                    compress_video(false);
                    for(int i = 0;i < 600 && ros::ok();i++){
                        std::this_thread::sleep_for(std::chrono::seconds(1));
                    }
                }
                std::cout << "compress video thread exit" << std::endl;
            }
        )
    );
    worker_threads_.emplace_back(
        std::make_shared<std::thread>(
            [this](){
                while(ros::ok()){
                    regular_clean();
                    for(int i = 0;i < 3600 && ros::ok();i++){
                        std::this_thread::sleep_for(std::chrono::seconds(1));
                    }
                }
                std::cout << "regular clean thread exit" << std::endl;
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

    if(save_files.size() == 0){
        return;
    }

    for(int i = 0; i < camera_config_["cameras"].size(); i++){
        float file_size = fs::file_size(save_files[i]) / 1024.0 / 1024.0;
        if(file_size > slice_){
            if(is_record){
                is_compress_record_files_[i].store(true);
            } else {
                is_compress_collect_files_[i].store(true);
            }
            for(int t = 0; t < 3 && ros::ok(); t++){
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
    if(!fs::exists(save_dir_)){
        return;
    }
    std::string cmd = fmt::format("{} {}/scripts/regular_clean.py --dataset {}", python_interpeter_, project_root_, save_dir_);
    int ret = std::system(cmd.c_str());
    if(ret != 0){
        ROS_ERROR_STREAM("\33[31mregular file cleanup failed\33[0m");
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
        fs::create_directories(save_dirs);
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
    }
    if(!is_running_collect){
        for(auto &writer_: collect_video_writers_){
            writer_->release();
        }
        collect_video_writers_.clear();
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
        cv::Mat image = cv_bridge::toCvShare(element->msg, element->msg->encoding)->image;
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
