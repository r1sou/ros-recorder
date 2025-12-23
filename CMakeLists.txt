cmake_minimum_required(VERSION 3.0.2)
project(recorder)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_FLAGS "-pthread -O3")

find_package(catkin REQUIRED COMPONENTS
  roscpp
  sensor_msgs
  std_msgs
  cv_bridge
  image_transport
)

find_package(OpenCV REQUIRED)
find_package(OpenSSL REQUIRED)
find_package(fmt REQUIRED)

include_directories(
    ${OpenCV_INCLUDE_DIRS}
    ${OpenSSL_INCLUDE_DIRS}
    ${catkin_INCLUDE_DIRS}
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/assets/3rdparty/ThreadPool/include
    ${CMAKE_CURRENT_SOURCE_DIR}/assets/3rdparty/nlohmann/include
    ${CMAKE_CURRENT_SOURCE_DIR}/assets/3rdparty/websocketpp/include
)

add_executable(${PROJECT_NAME}_node
    src/node.cpp
    src/main.cpp
)

target_link_libraries(${PROJECT_NAME}_node
    ${catkin_LIBRARIES}
    ${OpenCV_LIBRARIES}
    ${OpenSSL_LIBRARIES}
    fmt
    dl
    pthread
    stdc++fs
)

target_link_libraries(${PROJECT_NAME}_node
    OpenSSL::SSL
    OpenSSL::Crypto
)

catkin_package(
    CATKIN_DEPENDS roscpp sensor_msgs std_msgs cv_bridge image_transport
    DEPENDS OpenCV OpenSSL fmt stdc++fs
)

install(TARGETS ${PROJECT_NAME}_node RUNTIME DESTINATION ${CATKIN_PACKAGE_BIN_DESTINATION})

add_definitions(-DPROJECT_ROOT="${CMAKE_CURRENT_SOURCE_DIR}")
add_definitions(-DLAUNCH_FILE_PATH="${CMAKE_CURRENT_SOURCE_DIR}/assets/config/launch.json")
add_definitions(-DCAMERA_FILE_PATH="${CMAKE_CURRENT_SOURCE_DIR}/assets/config/camera.json")
add_definitions(-DCLIENT_FILE_PATH="${CMAKE_CURRENT_SOURCE_DIR}/assets/config/client.json")
