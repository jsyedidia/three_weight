cmake_minimum_required(VERSION 4.3.0)

project(twalib VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_CXX_SCAN_FOR_MODULES OFF)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Single-config Pixi/Ninja workflow knob. Change this one value to switch
# everything in build/cmake between Debug and Release.
set(TWALIB_BUILD_TYPE Release)
set(TWALIB_VALID_BUILD_TYPES Debug Release RelWithDebInfo MinSizeRel)
if (NOT TWALIB_BUILD_TYPE IN_LIST TWALIB_VALID_BUILD_TYPES)
  message(FATAL_ERROR "TWALIB_BUILD_TYPE must be one of: ${TWALIB_VALID_BUILD_TYPES}")
endif ()

if (NOT CMAKE_CONFIGURATION_TYPES)
  set(CMAKE_BUILD_TYPE "${TWALIB_BUILD_TYPE}" CACHE STRING "Build type" FORCE)
endif ()
message(STATUS "twalib build type: ${CMAKE_BUILD_TYPE}")

include(CTest)

set(TWALIB_RUNTIME_OUTPUT_DIR "${CMAKE_BINARY_DIR}/bin")
set(TWALIB_LIBRARY_OUTPUT_DIR "${CMAKE_BINARY_DIR}/lib")

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${TWALIB_RUNTIME_OUTPUT_DIR}")
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY "${TWALIB_LIBRARY_OUTPUT_DIR}")
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY "${TWALIB_LIBRARY_OUTPUT_DIR}")

set(TWALIB_CORE_SOURCES
    src/graph/factor_graph.cpp
    src/minimizers/in_range.cpp
    src/minimizers/known_value.cpp
    src/minimizers/one_hot.cpp
    src/minimizers/spy.cpp
    src/problems/circle_packing.cpp
    src/problems/compact_sudoku.cpp
    src/problems/sudoku.cpp)

function(twalib_apply_warnings target_name)
  if (MSVC)
    target_compile_options(${target_name} PRIVATE /W4 /permissive-)
  else()
    target_compile_options(${target_name} PRIVATE -Wall -Wextra -pedantic)
  endif ()
endfunction()

function(twalib_apply_debug_options target_name)
  if (MSVC)
    target_compile_options(${target_name} PRIVATE "$<$<CONFIG:Debug>:/Od;/Zi>")
  else()
    target_compile_options(${target_name} PRIVATE "$<$<CONFIG:Debug>:-O0;-g3;-fno-omit-frame-pointer>")
  endif ()
endfunction()

function(twalib_keep_test_assertions target_name)
  if (MSVC)
    target_compile_options(${target_name} PRIVATE /UNDEBUG)
  else()
    target_compile_options(${target_name} PRIVATE -UNDEBUG)
  endif ()
endfunction()

add_library(twalib_core STATIC ${TWALIB_CORE_SOURCES})
add_library(twalib::core ALIAS twalib_core)
target_include_directories(twalib_core PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/include")
target_include_directories(twalib_core PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/src")
target_compile_features(twalib_core PUBLIC cxx_std_23)
twalib_apply_warnings(twalib_core)
twalib_apply_debug_options(twalib_core)

add_executable(sudoku apps/sudoku.cpp)
target_link_libraries(sudoku PRIVATE twalib::core)
twalib_apply_warnings(sudoku)
twalib_apply_debug_options(sudoku)

function(twalib_add_test_executable target_name source_file)
  add_executable(${target_name} ${source_file})
  target_link_libraries(${target_name} PRIVATE twalib::core)
  twalib_apply_warnings(${target_name})
  twalib_keep_test_assertions(${target_name})
  if (BUILD_TESTING)
    add_test(NAME ${target_name} COMMAND ${target_name})
  endif ()
endfunction()

twalib_add_test_executable(graph_types_test tests/graph_types_test.cpp)
twalib_add_test_executable(edge_data_test tests/edge_data_test.cpp)
target_include_directories(edge_data_test PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/src")
twalib_add_test_executable(variable_data_test tests/variable_data_test.cpp)
target_include_directories(variable_data_test PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/src")
twalib_add_test_executable(factor_data_test tests/factor_data_test.cpp)
target_include_directories(factor_data_test PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/src")
twalib_add_test_executable(factor_graph_test tests/factor_graph_test.cpp)
twalib_add_test_executable(minimizers_test tests/minimizers_test.cpp)
twalib_add_test_executable(circle_packing_test tests/circle_packing_test.cpp)
twalib_add_test_executable(compact_sudoku_test tests/compact_sudoku_test.cpp)
target_compile_definitions(compact_sudoku_test PRIVATE TWALIB_SOURCE_DIR="${CMAKE_CURRENT_SOURCE_DIR}")
twalib_add_test_executable(sudoku_test tests/sudoku_test.cpp)

find_package(glfw3 CONFIG REQUIRED)
find_package(OpenGL REQUIRED)

find_path(
    TWALIB_IMGUI_INCLUDE_DIR
    NAMES imgui.h
    HINTS "$ENV{CONDA_PREFIX}/include" "${CMAKE_CURRENT_SOURCE_DIR}/.pixi/envs/default/include"
    REQUIRED)
find_library(
    TWALIB_IMGUI_LIBRARY
    NAMES imgui libimgui
    HINTS "$ENV{CONDA_PREFIX}/lib" "${CMAKE_CURRENT_SOURCE_DIR}/.pixi/envs/default/lib"
    REQUIRED)

if (TARGET glfw)
  set(TWALIB_GLFW_TARGET glfw)
elseif (TARGET glfw3::glfw)
  set(TWALIB_GLFW_TARGET glfw3::glfw)
else()
  message(FATAL_ERROR "The active environment did not provide a usable GLFW CMake target.")
endif ()

if (TARGET OpenGL::GL)
  set(TWALIB_OPENGL_TARGET OpenGL::GL)
elseif (TARGET OpenGL::OpenGL)
  set(TWALIB_OPENGL_TARGET OpenGL::OpenGL)
else()
  message(FATAL_ERROR "A usable OpenGL CMake target was not found.")
endif ()

add_executable(gui apps/gui.cpp)
target_include_directories(gui PRIVATE "${TWALIB_IMGUI_INCLUDE_DIR}")
target_link_libraries(
    gui
    PRIVATE
    twalib::core
    "${TWALIB_IMGUI_LIBRARY}"
    ${TWALIB_GLFW_TARGET}
    ${TWALIB_OPENGL_TARGET})
twalib_apply_warnings(gui)
twalib_apply_debug_options(gui)
