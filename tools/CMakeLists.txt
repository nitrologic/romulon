cmake_minimum_required(VERSION 3.25)

if(UNIX AND NOT APPLE)
    set(LINUX TRUE)
endif()

project(reset)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)


if(WIN32)

add_executable(rpc
	json.h
	enumports.cpp
	rpccoms.cpp
)
target_include_directories(rpc PRIVATE .)
target_link_libraries(rpc PRIVATE setupapi)

endif()
