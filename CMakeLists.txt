cmake_minimum_required(VERSION 3.5)
project(face_detect_project)

set(CMAKE_CXX_STANDARD 11)
find_package(OpenCV REQUIRED)
include_directories(${OpenCV_INCLUDE_DIRS} src)

# Server executable
add_executable(uqfacedetect src/uqfacedetect.cpp src/protocol.cpp)
target_link_libraries(uqfacedetect ${OpenCV_LIBS})

# Client executable
add_executable(uqfaceclient src/uqfaceclient.cpp src/protocol.cpp)
target_link_libraries(uqfaceclient ${OpenCV_LIBS})
