cmake_minimum_required(VERSION 3.12 FATAL_ERROR)
set(PNAME Reversi)
project(${PNAME})

set(CMAKE_INCLUDE_CURRENT_DIR ON)
set(CMAKE_AUTOMOC ON)
#set(CMAKE_AUTOUIC ON)
#set(CMAKE_AUTORCC ON)
set(CMAKE_PREFIX_PATH "C:/Qt/Qt5.9.7/5.9.7/msvc2015_64/lib/cmake/Qt5;C:/Tools/opencv/build/x64/vc14/lib")
#set(CMAKE_PREFIX_PATH "C:/Tools/Qt5.15.1/5.15.1/msvc2019_64/lib/cmake/Qt5;C:/Tools/opencv4.5.2./build/x64/vc14/lib")

find_package(Qt5 COMPONENTS Widgets REQUIRED)

set(CMAKE_CXX_FLAGS_COVERAGE "${CMAKE_CXX_FLAGS_RELEASE} -fprofile-arcs -ftest-coverage")

find_package( OpenCV REQUIRED )
include_directories( ${OpenCV_INCLUDE_DIRS} )

SET(TINYLOG_DIR "C:/Tools/TinyLog")
include_directories(${TINYLOG_DIR})

file( GLOB_RECURSE source_files src/* )
file( GLOB_RECURSE header_files include/* )

add_executable(${PNAME} WIN32
    ${source_files}
    ${header_files}
)

target_link_libraries(${PNAME} Qt5::Widgets)
target_link_libraries( ${PNAME} ${OpenCV_LIBS} )

#IF(CMAKE_BUILD_TYPE MATCHES DEBUG)
#	target_link_libraries(${PNAME} "C:/Tools/opencv/build/x64/vc15/lib/opencv_world341d.lib")
#ENDIF(CMAKE_BUILD_TYPE MATCHES DEBUG)

#IF(CMAKE_BUILD_TYPE MATCHES RELEASE)
#	target_link_libraries(${PNAME} "C:/Tools/opencv/build/x64/vc15/lib/opencv_world341.lib")
#ENDIF(CMAKE_BUILD_TYPE MATCHES DEBUG)


