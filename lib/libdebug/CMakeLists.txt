cmake_minimum_required(VERSION 2.8)
project(libdebug_project)

add_library(debug SHARED debug.c)

include_directories(../libdebug_hal)

install(TARGETS debug DESTINATION lib)
install(FILES debug.h DESTINATION include)
install(FILES debug_internal.h DESTINATION include)
