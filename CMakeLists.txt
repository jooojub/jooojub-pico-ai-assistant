cmake_minimum_required(VERSION 3.12)
include(pico_sdk_import.cmake)
project(pico_ai_assistant)
pico_sdk_init()

add_subdirectory(lib/Config)
add_subdirectory(lib/Infrared)
add_subdirectory(lib/OLED)
add_subdirectory(lib/Icm20948)
add_subdirectory(lib/LCD)
add_subdirectory(lib/Fonts)
add_subdirectory(lib/GUI)
add_subdirectory(examples)

include_directories(./src)
include_directories(./lib/Config)
include_directories(./lib/GUI)
include_directories(./lib/Icm20948)
include_directories(./lib/LCD)
include_directories(./lib/Infrared)
include_directories(./lib/OLED)
include_directories(./lib/Fonts)

add_executable(main
    main.c
    src/app.c
)

pico_enable_stdio_usb(main 1)
pico_enable_stdio_uart(main 1)

pico_add_extra_outputs(main)

target_link_libraries(main LCD OLED Infrared Icm20948 GUI Fonts Config pico_stdlib hardware_spi)
