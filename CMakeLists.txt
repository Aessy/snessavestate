project(serial_server)

cmake_minimum_required(VERSION 3.11)

SET(SRC usb2snes.cpp
        main.cpp)

add_executable(save_state ${SRC})
target_compile_options(save_state PRIVATE -std=c++17 -g)
target_link_libraries(save_state serialport boost_system pthread)
