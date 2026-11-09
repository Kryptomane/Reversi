cmake_minimum_required(VERSION 3.19)
project(reversi VERSION 1.0.7 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)

find_package(Qt6 REQUIRED COMPONENTS Widgets)
find_package(Threads REQUIRED)

# Spiellogik und Engine ohne Qt (auch fuer die Tests)
set(ENGINE_SOURCES
    src/engine.cpp
    src/evaluation.cpp
    src/game.cpp
)

qt_add_executable(reversi WIN32
    src/main.cpp
    src/reversigui.cpp
    src/boardwidget.cpp
    src/messageoverlay.cpp
    ${ENGINE_SOURCES}
    include/bitboard.h
    include/boardwidget.h
    include/messageoverlay.h
    include/engine.h
    include/evaluation.h
    include/game.h
    include/reversigui.h
    include/transpositiontable.h
    include/ui_reversigui.h
)
target_include_directories(reversi PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_link_libraries(reversi PRIVATE Qt6::Widgets Threads::Threads)
target_compile_definitions(reversi PRIVATE APP_VERSION="${PROJECT_VERSION}")

# Android: Versionscode wird aus der Version berechnet (1.0.2 -> 10002) und steigt so automatisch.
math(EXPR ANDROID_VERSION_CODE "${PROJECT_VERSION_MAJOR} * 10000 + ${PROJECT_VERSION_MINOR} * 100 + ${PROJECT_VERSION_PATCH}")
set_target_properties(reversi PROPERTIES
    QT_ANDROID_APP_NAME "Reversi"
    QT_ANDROID_PACKAGE_NAME "de.maile.reversi"
    QT_ANDROID_VERSION_NAME "${PROJECT_VERSION}"
    QT_ANDROID_VERSION_CODE ${ANDROID_VERSION_CODE}
    QT_ANDROID_MIN_SDK_VERSION 28
    # Bibliotheken komprimiert in die APK packen: APK etwa halb so gross (~14 statt ~31 MB),
    # dafuer werden sie bei der Installation entpackt (etwas mehr Speicher auf dem Geraet).
    QT_ANDROID_LEGACY_PACKAGING TRUE
)
# Das Spiel zeichnet alles selbst und laedt keine Bilder/Icons: diese Plugins nicht mitliefern.
qt_import_plugins(reversi EXCLUDE_BY_TYPE imageformats iconengines)

if(NOT ANDROID)
    # Tests (perft, Spielablauf, Endspiel exakt, Stufen) und Benchmark - im Release-Build laufen lassen.
    foreach(tool engine_test benchmark)
        add_executable(${tool} tests/${tool}.cpp ${ENGINE_SOURCES})
        target_include_directories(${tool} PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include ${CMAKE_CURRENT_SOURCE_DIR}/tests)
        target_link_libraries(${tool} PRIVATE Threads::Threads)
    endforeach()
endif()
