cmake_minimum_required(VERSION 3.10)
project(LrccExpt)

set(CMAKE_CXX_STANDARD 11)

include_directories(sqlite3)
link_directories(sqlite3)
link_libraries(sqlite3)

include_directories(json)
link_directories(json)
link_libraries(json)


add_library(
        sqlite3
        sqlite3/sqlite3.h
        sqlite3/sqlite3.c
        sqlite3/sqlite3ext.h
)
add_library(
        json
        json/json.h
        json/jsoncpp.cpp
)

add_executable(LrccExpt main.cpp)
