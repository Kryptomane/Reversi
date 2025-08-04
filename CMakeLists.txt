cmake_minimum_required(VERSION 3.19)
project(reversi LANGUAGES CXX)

# Setze den Pfad zu OpenCV, falls nicht automatisch gefunden
set(OpenCV_DIR "C:/Tools/opencv/build/x64/vc15/lib/")

# Finde OpenCV
find_package(Qt6 COMPONENTS Widgets REQUIRED)
find_package(OpenCV REQUIRED)

# Stelle sicher, dass CMake automatisch MOC, UIC und RCC behandelt (optional für Qt)
set(CMAKE_AUTOMOC ON)
#set(CMAKE_AUTOUIC ON)
#set(CMAKE_AUTORCC ON)

# Optional: Falls du OpenCV-Includes manuell angeben musst (nicht zwingend nötig, wenn `find_package` funktioniert)
# include_directories(${OpenCV_INCLUDE_DIRS})

# Füge den Pfad zu TinyLog hinzu
SET(TINYLOG_DIR "C:/Tools/TinyLog")
include_directories(${TINYLOG_DIR})

# Quell- und Header-Dateien einfügen
file(GLOB_RECURSE source_files src/*)
file(GLOB_RECURSE header_files include/*)

# Erstelle das Executable
qt_add_executable(reversi WIN32
    ${source_files}
    ${header_files}
)

# Linke mit den entsprechenden Bibliotheken
target_link_libraries(reversi PRIVATE
    Qt6::Widgets
    ${OpenCV_LIBS}  # OpenCV Bibliotheken
)

target_include_directories(reversi PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include)

# Wenn du Debug-/Release-Bibliotheken spezifisch verlinken willst, kannst du das wie folgt tun:
# IF(CMAKE_BUILD_TYPE MATCHES DEBUG)
#     target_link_libraries(reversi PRIVATE "C:/Tools/opencv/build/x64/vc15/lib/opencv_world341d.lib")
# ENDIF(CMAKE_BUILD_TYPE MATCHES DEBUG)

# IF(CMAKE_BUILD_TYPE MATCHES RELEASE)
#     target_link_libraries(reversi PRIVATE "C:/Tools/opencv/build/x64/vc15/lib/opencv_world341.lib")
# ENDIF(CMAKE_BUILD_TYPE MATCHES RELEASE)
