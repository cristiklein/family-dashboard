cmake_minimum_required(VERSION 3.30)
project(family_dashboard LANGUAGES CXX)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Build the same pinned YAML parser for macOS and the statically linked Pi binary.
include(FetchContent)
set(YAML_CPP_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(YAML_CPP_BUILD_TOOLS OFF CACHE BOOL "" FORCE)
set(YAML_CPP_INSTALL OFF CACHE BOOL "" FORCE)
set(YAML_CPP_FORMAT_SOURCE OFF CACHE BOOL "" FORCE)
set(YAML_BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)
set(yaml_archive "${CMAKE_CURRENT_SOURCE_DIR}/build/dependencies/yaml-cpp-0.8.0.tar.gz")
if(NOT EXISTS "${yaml_archive}")
  set(yaml_archive "https://codeload.github.com/jbeder/yaml-cpp/tar.gz/refs/tags/0.8.0")
endif()
FetchContent_Declare(yaml-cpp
  URL "${yaml_archive}"
  URL_HASH SHA256=fbe74bbdcee21d656715688706da3c8becfd946d92cd44705cc6098bb23b3a16)
block()
  # yaml-cpp 0.8.0 predates CMake 4's removal of pre-3.5 compatibility.
  set(CMAKE_POLICY_VERSION_MINIMUM 3.10)
  FetchContent_MakeAvailable(yaml-cpp)
endblock()

add_executable(family-dashboard src/main.cpp)
target_link_libraries(family-dashboard PRIVATE yaml-cpp::yaml-cpp)
target_compile_features(family-dashboard PRIVATE cxx_std_26)
set_target_properties(family-dashboard PROPERTIES CXX_EXTENSIONS OFF)
target_sources(family-dashboard PRIVATE
  FILE_SET CXX_MODULES FILES
    src/greeting.cppm src/framebuffer.cppm src/display.cppm src/config.cppm src/layout.cppm)

# Deploy a standalone executable without target-side C++ runtime dependencies.
if(CMAKE_CROSSCOMPILING)
  target_link_options(family-dashboard PRIVATE -static)
endif()

include(CTest)
if(BUILD_TESTING AND NOT CMAKE_CROSSCOMPILING)
  add_executable(framebuffer-test tests/framebuffer_test.cpp)
  target_compile_features(framebuffer-test PRIVATE cxx_std_26)
  set_target_properties(framebuffer-test PROPERTIES CXX_EXTENSIONS OFF)
  target_sources(framebuffer-test PRIVATE
    FILE_SET CXX_MODULES FILES src/framebuffer.cppm)
  add_test(NAME framebuffer COMMAND framebuffer-test)

  add_executable(config-test tests/config_test.cpp)
  target_link_libraries(config-test PRIVATE yaml-cpp::yaml-cpp)
  target_compile_features(config-test PRIVATE cxx_std_26)
  set_target_properties(config-test PROPERTIES CXX_EXTENSIONS OFF)
  target_sources(config-test PRIVATE
    FILE_SET CXX_MODULES FILES src/config.cppm)
  add_test(NAME config COMMAND config-test)

  add_executable(layout-test tests/layout_test.cpp)
  target_link_libraries(layout-test PRIVATE yaml-cpp::yaml-cpp)
  target_compile_features(layout-test PRIVATE cxx_std_26)
  set_target_properties(layout-test PROPERTIES CXX_EXTENSIONS OFF)
  target_sources(layout-test PRIVATE
    FILE_SET CXX_MODULES FILES src/framebuffer.cppm src/config.cppm src/layout.cppm)
  add_test(NAME layout COMMAND layout-test)
  set_tests_properties(layout PROPERTIES ENVIRONMENT "TZ=UTC")
  add_test(NAME layout-stockholm COMMAND layout-test)
  set_tests_properties(layout-stockholm PROPERTIES ENVIRONMENT "TZ=Europe/Stockholm")
endif()
