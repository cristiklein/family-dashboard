cmake_minimum_required(VERSION 3.30)
project(family_dashboard LANGUAGES CXX)

add_executable(family-dashboard src/main.cpp)
target_compile_features(family-dashboard PRIVATE cxx_std_26)
set_target_properties(family-dashboard PROPERTIES CXX_EXTENSIONS OFF)
target_sources(family-dashboard PRIVATE
  FILE_SET CXX_MODULES FILES
    src/greeting.cppm src/framebuffer.cppm src/display.cppm)

# This first smoke test needs no shared libraries on an older TinkerOS.
if(CMAKE_CROSSCOMPILING)
  target_link_options(family-dashboard PRIVATE -static)
endif()

add_custom_target(format
  COMMAND bash "${CMAKE_CURRENT_SOURCE_DIR}/scripts/format.sh"
  WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
  COMMENT "Format C++ sources using Allman braces")
add_custom_target(format-check
  COMMAND bash "${CMAKE_CURRENT_SOURCE_DIR}/scripts/format.sh" --check
  WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
  COMMENT "Check C++ formatting")
add_dependencies(family-dashboard format-check)
