cmake_minimum_required(VERSION 3.20)
project(recorder)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_CXX_FLAGS "-pthread -O3")

find_package(fmt REQUIRED)
find_package(OpenSSL REQUIRED)
find_package(OpenCV REQUIRED)

find_package(ament_cmake REQUIRED)
find_package(rclcpp REQUIRED)
find_package(sensor_msgs REQUIRED)
find_package(std_msgs REQUIRED)
find_package(cv_bridge REQUIRED)

include_directories(
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/assets/3rdparty/nlohmann/include
    ${CMAKE_CURRENT_SOURCE_DIR}/assets/3rdparty/websocketpp/include
)

function(add_project_executable target_name)
    add_executable(${target_name} ${ARGN})
    ament_target_dependencies(${target_name}
        rclcpp
        cv_bridge
        std_msgs
        sensor_msgs
    )

    target_link_libraries(${target_name}
        ${OpenCV_LIBRARIES}
        ${OPENSSL_LIBRARIES}
        pthread
        fmt
    )

    install(TARGETS ${target_name}
        DESTINATION lib/${PROJECT_NAME}
    )
endfunction()

file(GLOB SRC_FILES 
    ${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp
)

add_project_executable(${PROJECT_NAME}_node
    ${SRC_FILES}
)

file(GLOB LAUNCH_FILES
    ${CMAKE_CURRENT_SOURCE_DIR}/launch/launch.py
)
install(FILES ${LAUNCH_FILES} DESTINATION share/${PROJECT_NAME}/launch)
install(DIRECTORY assets/config DESTINATION share/${PROJECT_NAME})
install(DIRECTORY scripts DESTINATION share/${PROJECT_NAME})
install(DIRECTORY launch DESTINATION share/${PROJECT_NAME})

ament_package()
