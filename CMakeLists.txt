cmake_minimum_required(VERSION 3.13.4)
project(ttyM)
include_directories( ${/usr/include/})
add_executable(serial serial.cpp)
target_link_options(serial PUBLIC "-lpthread")

