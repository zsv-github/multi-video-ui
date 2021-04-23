cmake_minimum_required(VERSION 3.5)

project(face-ui)
set(CMAKE_BUILD_TYPE Release)

set(USE_TRT OFF)
set(LOG_TIME ON)
set(USE_JETSON_UTILS OFF)


if (USE_TRT)
    add_definitions(-DUSE_TRT)
endif ()

if (LOG_TIME)
    add_definitions(-DLOG_TIME)
endif ()

if (USE_JETSON_UTILS)
    add_definitions(-DUSE_JETSON_UTILS)
endif ()

message(STATUS "Processor: ${CMAKE_SYSTEM_PROCESSOR}")

if (CMAKE_SYSTEM_PROCESSOR MATCHES "(x86_64)")
    set(x86_64 TRUE)
elseif (CMAKE_SYSTEM_PROCESSOR MATCHES "(aarch64)")
    set(aarch64 TRUE)
endif ()

execute_process(COMMAND sh gen.sh WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/face_recognition/src/proto)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)

find_package(QT NAMES Qt5 COMPONENTS Widgets LinguistTools REQUIRED)
find_package(Qt${QT_VERSION_MAJOR} COMPONENTS Widgets LinguistTools REQUIRED)
find_package(OpenCV REQUIRED)
find_package(Qt5Widgets REQUIRED)
find_package(Threads REQUIRED)


find_package(OpenMP)
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -fopenmp")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${OpenMP_CXX_FLAGS}")

if (x86_64)
    find_package(Protobuf 3.14.0.0 PATHS ${CMAKE_CURRENT_SOURCE_DIR}/face_recognition/libs/gRPC REQUIRED)
    find_package(gRPC 1.35.0 CONFIG PATHS ${CMAKE_CURRENT_SOURCE_DIR}/face_recognition/libs/gRPC REQUIRED)
    find_package(OpenCV 4.5.2 EXACT PATHS ${CMAKE_CURRENT_SOURCE_DIR}/face_recognition/libs REQUIRED)
elseif (aarch64)
    find_package(Protobuf REQUIRED)
    find_package(gRPC REQUIRED)
    find_package(OpenCV REQUIRED)
    find_package(CUDA REQUIRED)
endif ()

set(GRPC_CPP_LIB gRPC::grpc++_unsecure)
set(GRPC_PLUGIN_EXEC $<TARGET_FILE:gRPC::grpc_cpp_plugin>)
set(THREADS_PREFER_PTHREAD_FLAG ON)

set(TS_FILES face-ui_en_US.ts)

set(PROJECT_SOURCES
        main.cpp
        mainwindow.cpp
        mainwindow.h
        mainwindow.ui
        ${TS_FILES}
)

set(FACE_RECOGNITION_SOURCES
        face_recognition/src/face_recognition.h
        face_recognition/src/faiss_service.h
        face_recognition/src/FaceDetector.h
        face_recognition/src/aligner.h
        face_recognition/src/aligner.cpp
        face_recognition/src/proto/multiple_camera_server.grpc.pb.h
        face_recognition/src/proto/multiple_camera_server.grpc.pb.cc
        face_recognition/src/proto/multiple_camera_server.pb.h
        face_recognition/src/proto/multiple_camera_server.pb.cc
        face_recognition/src/grpc_server.h)

add_executable(
        face-ui
        ${PROJECT_SOURCES}
        ${FACE_RECOGNITION_SOURCES}
)

qt5_create_translation(QM_FILES ${CMAKE_SOURCE_DIR} ${TS_FILES})

include_directories(
        ${OpenCV_INCLUDE_DIRS}
        ${CMAKE_CURRENT_SOURCE_DIR}/face_recognition/src
        ${CMAKE_CURRENT_SOURCE_DIR}/face_recognition/src/proto
        /usr/local/include/jetson-utils
)

if (x86_64)
    include_directories(
            ${CMAKE_CURRENT_SOURCE_DIR}/face_recognition/libs/faiss/include
    )
    if (USE_TRT)
        include_directories(
                ${CMAKE_CURRENT_SOURCE_DIR}/face_recognition/libs/onnxruntime-ubuntu18.04-x64-trt7.2.2-1+cuda11.0/include/onnxruntime/core/session
        )
    else ()
        include_directories(
                ${CMAKE_CURRENT_SOURCE_DIR}/face_recognition/libs/onnxruntime-linux-x64-1.7.0/include
        )
    endif ()
elseif (aarch64)
    include_directories(
            /usr/local/include/faiss
            /usr/local/include/onnxruntime/core/session
            /usr/local/cuda/include
    )
endif ()


if (x86_64)
    target_link_libraries(
            face-ui
            Qt${QT_VERSION_MAJOR}::Widgets
            Threads::Threads
            -lboost_filesystem -lboost_system
            -lstdc++fs
            ${CMAKE_CURRENT_SOURCE_DIR}/face_recognition/libs/faiss/lib/libfaiss.a
            -lblas
            -llapack
            -lgfortran
            ${OpenCV_LIBS}
            ${GRPC_CPP_LIB}
            ${Protobuf_LIBRARIES}
    )
    if (USE_TRT)
        target_link_libraries(
                face-ui

                ${CMAKE_CURRENT_SOURCE_DIR}/face_recognition/libs/onnxruntime-ubuntu18.04-x64-trt7.2.2-1+cuda11.0/lib/libonnxruntime.so
                ${CMAKE_CURRENT_SOURCE_DIR}/face_recognition/libs/onnxruntime-ubuntu18.04-x64-trt7.2.2-1+cuda11.0/lib/libonnxruntime_providers_shared.so
                ${CMAKE_CURRENT_SOURCE_DIR}/face_recognition/libs/onnxruntime-ubuntu18.04-x64-trt7.2.2-1+cuda11.0/lib/libonnxruntime_providers_tensorrt.so
        )
    else ()
        target_link_libraries(
                face-ui
                ${CMAKE_CURRENT_SOURCE_DIR}/face_recognition/libs/onnxruntime-linux-x64-1.7.0/lib/libonnxruntime.so
        )
    endif ()
elseif (aarch64)
    target_link_libraries(
            face-ui
            Threads::Threads
            -lboost_filesystem -lboost_system
            -lstdc++fs
            -lfaiss
            -lblas
            -llapack
            -ljetson-utils
            ${OpenCV_LIBS}
            ${GRPC_CPP_LIB}
            ${Protobuf_LIBRARIES}
            -lonnxruntime
    )
endif ()
