# RP2040 picotool build chain for romulon

cmake_minimum_required(VERSION 4.2)

set(PICO_BOARD pico)
set(PICO_PLATFORM rp2040)
# set(PICO_FLASH_SIZE_BYTES 1048576)

set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)

include($ENV{PICO_SDK_PATH}/pico_sdk_init.cmake)

add_compile_definitions(PICO_STDIO_USB_ENABLE_RESET_VIA_BAUD_RATE=0)
add_compile_definitions(PICO_STDIO_USB_ENABLE_RESET_VIA_VENDOR_INTERFACE=0)

#add_compile_options(-Wall -Wextra)
#add_compile_options(-Wall)
add_compile_options(-Wno-missing-field-initializers)
add_compile_options(-Wno-unused-variable)
add_compile_options(-Wno-unused-parameter)
add_compile_options(-Wno-unused-function)
add_compile_options(-Wno-unused-but-set-parameter)

# add_compile_definitions(LCD_2_8_touch)

project(romulon C CXX ASM)

pico_sdk_init()

add_executable(romulon
    picosdk/picosdk.cpp
	romulon.cpp
)

pico_enable_stdio_usb(romulon 1)
pico_enable_stdio_uart(romulon 0)

target_link_libraries(romulon
	pico_util
	pico_stdlib 
	pico_rand
	pico_multicore 
	hardware_rtc 
	hardware_spi 
	hardware_adc 
	hardware_dma 
	hardware_pio 
#	hardware_uart
	hardware_i2c 
	hardware_pwm
)

pico_add_extra_outputs(romulon)
# set_target_properties(romulon PROPERTIES OUTPUT_NAME "romulon")
