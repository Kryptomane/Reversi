cmake_minimum_required(VERSION 3.19)
project(reversi VERSION 1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)          # std::popcount / std::countr_zero im Bitboard-Kern
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Qt Widgets (Desktop und Android), Threads fuer die parallele Suche. Kein OpenCV mehr.
find_package(Qt6 REQUIRED COMPONENTS Widgets)
find_package(Threads REQUIRED)

set(CMAKE_AUTOMOC ON)

# Quell- und Header-Dateien
file(GLOB_RECURSE source_files src/*.cpp)
file(GLOB_RECURSE header_files include/*.h)

qt_add_executable(reversi WIN32
    ${source_files}
    ${header_files}
)

target_include_directories(reversi PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}          # TinyLog.h liegt im Projektordner
)

target_link_libraries(reversi PRIVATE
    Qt6::Widgets
    Threads::Threads
)

# Android: App-Name, Paket-ID und Version (werden auf dem Desktop ignoriert)
set_target_properties(reversi PROPERTIES
    QT_ANDROID_APP_NAME "Reversi"
    QT_ANDROID_PACKAGE_NAME "de.maile.reversi"
    QT_ANDROID_VERSION_NAME "${PROJECT_VERSION}"
    QT_ANDROID_VERSION_CODE 1
    QT_ANDROID_MIN_SDK_VERSION 28
)

if(NOT ANDROID)
    # Vergleichstest + Benchmark des Zuggenerators (ohne Qt). Im Release-Build laufen lassen.
    add_executable(movegen_test tests/movegen_test.cpp)
    target_include_directories(movegen_test PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include ${CMAKE_CURRENT_SOURCE_DIR}/tests)

    # Engine-Test: Endspiel exakt gegen Brute-Force, Partien gegen Zufall/Gierig, Geschwindigkeit.
    add_executable(engine_test tests/engine_test.cpp src/engine.cpp)
    target_link_libraries(engine_test PRIVATE Threads::Threads)
    target_include_directories(engine_test PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include)
endif()
