cmake_minimum_required(VERSION 3.10)

project(CameraStream)

set(CMAKE_CXX_STANDARD 17)

find_package(PkgConfig)
pkg_check_modules(libcamera REQUIRED libcamera)
find_package(OpenCV REQUIRED)

add_executable(camera_stream camera_stream.cpp)
target_link_libraries(camera_stream PRIVATE ${libcamera_LINK_LIBRARIES} ${OpenCV_LIBS})
target_include_directories(camera_stream PUBLIC ${libcamera_INCLUDE_DIRS})
