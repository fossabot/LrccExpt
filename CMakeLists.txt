cmake_minimum_required(VERSION 3.10)
project(LrccExpt)

IF (CMAKE_BUILD_TYPE MATCHES "Release")
    SET(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -O3")
    SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -O3")
ENDIF ()

set(CMAKE_CXX_STANDARD 11)

include_directories(sqlite3)
link_directories(sqlite3)
link_libraries(sqlite3)

add_library(
        json
        json/json.h
        json/jsoncpp.cpp
)

add_executable(LrccExpt main.cpp)
target_link_libraries(LrccExpt json)