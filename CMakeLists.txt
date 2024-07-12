cmake_minimum_required(VERSION 3.5)

project(multi-cam)

# Find includes in corresponding build directories
set(CMAKE_INCLUDE_CURRENT_DIR ON)
# Instruct CMake to run moc automatically when needed.
set(CMAKE_AUTOMOC ON)

# Qt libs
find_package(QT NAMES Qt5 COMPONENTS Widgets LinguistTools REQUIRED)
find_package(Qt${QT_VERSION_MAJOR} COMPONENTS Widgets LinguistTools REQUIRED)
find_package(OpenCV REQUIRED)
find_package(Qt5Widgets REQUIRED)
find_package(Threads REQUIRED)

# Openmp
find_package(OpenMP)
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -fopenmp")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${OpenMP_CXX_FLAGS}")

# OpenCV
find_package(OpenCV PATHS /home/local/Mine/libraries/opencv_cpp REQUIRED)
include_directories(${OpenCV_INCLUDE_DIRS})

# Multi-threading
set(THREADS_PREFER_PTHREAD_FLAG ON)

# target_link_libraries(
#     multi-cam
#     Qt${QT_VERSION_MAJOR}::Widgets
#     Threads::Threads
#     -lboost_filesystem -lboost_system
#     -lstdc++fs
#         ${OpenCV_LIBS}
#         ${OpenCV_LIBS}
#         ${GRPC_CPP_LIB}
#         ${Protobuf_LIBRARIES}
#     ${OpenCV_LIBS}
#         ${GRPC_CPP_LIB}
#         ${Protobuf_LIBRARIES}
# )

add_subdirectory(src)
