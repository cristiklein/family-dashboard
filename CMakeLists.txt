cmake_minimum_required(VERSION 3.30)
project(family_dashboard LANGUAGES CXX)

add_executable(family-dashboard src/main.cpp)
target_compile_features(family-dashboard PRIVATE cxx_std_26)
set_target_properties(family-dashboard PROPERTIES CXX_EXTENSIONS OFF)
target_sources(family-dashboard PRIVATE
  FILE_SET CXX_MODULES FILES src/greeting.cppm)

# This first smoke test needs no shared libraries on an older TinkerOS.
if(CMAKE_CROSSCOMPILING)
  target_link_options(family-dashboard PRIVATE -static)
endif()
