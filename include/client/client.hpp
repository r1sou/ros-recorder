// #pragma once

// #include "websocketpp/client.hpp"
// #include "websocketpp/config/asio_no_tls_client.hpp"

// #include <openssl/evp.h>
// #include <openssl/hmac.h>
// #include <openssl/bio.h>
// #include <openssl/buffer.h>

// #include "common.h"

// using Client = websocketpp::client<websocketpp::config::asio_client>;

// class BasicWebSocketClient
// {
// public:
//     BasicWebSocketClient(const std::string uri): uri(uri) {
//         m_client.clear_access_channels(websocketpp::log::alevel::all);
//         m_client.clear_error_channels(websocketpp::log::elevel::all);

//         m_client.init_asio();
//         m_client.start_perpetual();

//         m_client.set_open_handler(
//             [this](websocketpp::connection_hdl hdl){
//                 this->on_open(hdl); 
//             }
//         );
//         m_client.set_close_handler(
//             [this](websocketpp::connection_hdl hdl){ 
//                 this->on_close(hdl); 
//             }
//         );
//         m_client.set_fail_handler(
//             [this](websocketpp::connection_hdl hdl){
//                 this->on_fail(hdl); 
//             }
//         );
//         // m_client.set_message_handler(
//         //     [this](websocketpp::connection_hdl hdl, websocketpp::config::asio_client::message_type::ptr msg){
//         //         this->on_message(hdl, msg); 
//         //     }
//         // );
//     }
//     ~BasicWebSocketClient(){
//         m_client.stop_perpetual();
//         websocketpp::lib::error_code ec;
//         m_client.close(m_handle, websocketpp::close::status::normal, "close", ec);
//         if (m_thread->joinable())
//         {
//             m_thread->join();
//         }
//         if (m_reconnect_thread_.joinable())
//         {
//             m_reconnect_thread_.join();
//         }
//     }
// public:
//     void on_open(websocketpp::connection_hdl hdl){
//         std::string info = fmt::format("\33[32mConnection uri {} opened\33[0m", uri);
//         RCLCPP_INFO_STREAM(rclcpp::get_logger("WebSocketClient"), info.c_str());
//         connected.store(true);
//     }
//     void on_fail(websocketpp::connection_hdl hdl){
//         std::string info = fmt::format("\33[31mConnection uri {} failed\33[0m", uri);
//         RCLCPP_INFO_STREAM(rclcpp::get_logger("WebSocketClient"), info.c_str());
//     }
//     void on_close(websocketpp::connection_hdl hdl){
//         std::string info = fmt::format("\33[31mConnection uri {} closed\33[0m", uri);
//         RCLCPP_INFO_STREAM(rclcpp::get_logger("WebSocketClient"), info.c_str());
//         connected.store(false);
//     }
//     virtual void on_message(websocketpp::connection_hdl hdl, Client::message_ptr msg) = 0;

//     void start(){
//         m_thread = websocketpp::lib::make_shared<websocketpp::lib::thread>(&Client::run, &m_client);

//         connect();

//         m_reconnect_thread_ = std::thread(
//             [this](){
//                 std::this_thread::sleep_for(std::chrono::seconds(3));
//                 while(rclcpp::ok() && !reconnecting.load()){
//                     for(int i = 0; i < 3 && rclcpp::ok(); i++){
//                         std::this_thread::sleep_for(std::chrono::seconds(1));
//                     }
//                     if (connected.load()){
//                         continue;
//                     }
//                     reconnecting.store(true);
//                     std::string info = fmt::format("\33[33mReconnecting to {}...\33[0m", this->uri);
//                     RCLCPP_INFO_STREAM(rclcpp::get_logger("WebSocketClient"), info.c_str());
//                     connect();
//                     reconnecting.store(false);
//                 }
//                 RCLCPP_INFO_STREAM(rclcpp::get_logger("WebSocketClient"), "websocket Reconnect thread exiting");
//             }
//         );
//         m_reconnect_thread_.detach();
//     }

//     void connect(){
//         websocketpp::lib::error_code ec;
//         auto con = m_client.get_connection(uri, ec);
//         if (ec)
//         {
//             RCLCPP_ERROR_STREAM(rclcpp::get_logger("WebSocketClient"), "Error: " << ec.message());
//             return;
//         }
//         m_handle = con->get_handle();
//         m_client.connect(con);
//     }
//     void send_message(const std::string message){
//         // RCLCPP_INFO_STREAM(rclcpp::get_logger("WebSocketClient"), "Sending message: " << message);
//         m_client.send(m_handle, message, websocketpp::frame::opcode::text);
//     }
// public:
//     std::string uri;
//     std::atomic<bool> connected{false};
//     std::atomic<bool> reconnecting{false};
// public:
//     Client m_client;
//     websocketpp::connection_hdl m_handle;
//     websocketpp::lib::shared_ptr<websocketpp::lib::thread> m_thread;

//     std::thread m_reconnect_thread_;
// };


// class RecorderClient: public BasicWebSocketClient
// {
// public:
//     RecorderClient(const std::string uri): BasicWebSocketClient(uri) {
//         m_client.set_message_handler(
//             [this](websocketpp::connection_hdl hdl, websocketpp::config::asio_client::message_type::ptr msg){
//                 this->on_message(hdl, msg); 
//             }
//         );
//     }
//     ~RecorderClient() = default;
// public:
//     void on_message(websocketpp::connection_hdl hdl, Client::message_ptr msg) override{
//         std::string message = msg->get_payload();
//         nlohmann::json data = nlohmann::json::parse(message);

//         if(data["result_code"].is_null() || data["result_code"] != 0){
//             return;
//         }

//         // ROS_INFO_STREAM("Received message: " << data.dump(4));

//         if(data["cmd_code"] != 0x14){
//             return;
//         }

//         start_collect.store(data["data"]["collect_cam_dat_op"] == 1);
//         start_record.store(data["data"]["cam_record_op"] == 1);
//     }
// public:
//     std::atomic<bool> start_collect{false};
//     std::atomic<bool> start_record{false};
// };

// class JWTGenerator
// {
// public:
//     static std::string generate(std::string req_id, std::string secret){
//         nlohmann::json header = {
//             {"alg", "HS256"},
//             {"typ", "JWS"}
//         };

//         auto now = std::chrono::duration_cast<std::chrono::seconds>(
//                     std::chrono::system_clock::now().time_since_epoch())
//                     .count();

//         nlohmann::json payload = {
//             {"aud", "koko robot"},
//             {"exp", now + 3600},
//             {"iss", "www.kokobots.com"},
//             {"req_id", req_id},
//             {"sub", "robot access token"}};

//         std::string header_str = header.dump();
//         std::string payload_str = payload.dump();

//         std::string encoded_header = base64url_encode(reinterpret_cast<const unsigned char *>(header_str.data()), header_str.size());
//         std::string encoded_payload = base64url_encode(reinterpret_cast<const unsigned char *>(payload_str.data()), payload_str.size());

//         std::string signing_input = encoded_header + "." + encoded_payload;

//         unsigned char hash[32];
//         unsigned int hash_len;
//         HMAC(EVP_sha256(),
//             secret.c_str(), secret.length(),
//             reinterpret_cast<const unsigned char *>(signing_input.c_str()), signing_input.length(),
//             hash, &hash_len);

//         std::string encoded_signature = base64url_encode(hash, hash_len);

//         return signing_input + "." + encoded_signature;
//     }
//     static std::string base64url_encode(const unsigned char *data, size_t len){
//         BIO *b64 = BIO_new(BIO_f_base64());
//         BIO *bio = BIO_new(BIO_s_mem());
//         bio = BIO_push(b64, bio);

//         BIO_set_flags(bio, BIO_FLAGS_BASE64_NO_NL);
//         BIO_write(bio, data, len);
//         BIO_flush(bio);

//         char *encoded_data = nullptr;
//         long length = BIO_get_mem_data(bio, &encoded_data);

//         std::string result(encoded_data, length);

//         std::string output;
//         for (char c : result)
//         {
//             if (c == '+')
//                 output += '-';
//             else if (c == '/')
//                 output += '_';
//             else if (c != '=')
//                 output += c;
//         }

//         BIO_free_all(bio);
//         return output;
//     }
// };
