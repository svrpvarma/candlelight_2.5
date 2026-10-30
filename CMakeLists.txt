cmake_minimum_required(VERSION 3.16)
project(qtcandlelightfdcanbus LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)

# Qt 5.15 (SavvyCAN V213/V220) or Qt 6 (SavvyCAN Qt6 builds) - whichever is found first
find_package(QT NAMES Qt5 Qt6 REQUIRED COMPONENTS Core SerialBus)
find_package(Qt${QT_VERSION_MAJOR} REQUIRED COMPONENTS Core SerialBus)
message(STATUS "Building candleLightFD plugin for Qt ${QT_VERSION}")

add_library(qtcandlelightfdcanbus MODULE
    plugin.cpp plugin.hpp
    device.cpp device.hpp
    devices.cpp devices.hpp
    interface.cpp interface.hpp
    listener.cpp listener.hpp
    timing.cpp timing.hpp
    api/candle.c api/candle.h
    api/candle_ctrl_req.c api/candle_ctrl_req.h
    api/candle_defs.h api/ch_9.h
    plugin.json
)

target_compile_definitions(qtcandlelightfdcanbus PRIVATE UNICODE _UNICODE QT_PLUGIN)
if(QT_VERSION_MAJOR EQUAL 5)
    target_compile_definitions(qtcandlelightfdcanbus PRIVATE CANDLE_QT5)
endif()
target_link_libraries(qtcandlelightfdcanbus PRIVATE Qt${QT_VERSION_MAJOR}::Core Qt${QT_VERSION_MAJOR}::SerialBus)
if(WIN32)
    target_link_libraries(qtcandlelightfdcanbus PRIVATE setupapi ole32 winusb)
endif()
