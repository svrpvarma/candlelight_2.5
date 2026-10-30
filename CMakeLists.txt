cmake_minimum_required(VERSION 3.16)
project(qtcandlelightfdcanbus LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)

find_package(Qt6 REQUIRED COMPONENTS Core SerialBus)

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
target_link_libraries(qtcandlelightfdcanbus PRIVATE Qt6::Core Qt6::SerialBus)
if(WIN32)
    target_link_libraries(qtcandlelightfdcanbus PRIVATE setupapi ole32 winusb)
endif()
