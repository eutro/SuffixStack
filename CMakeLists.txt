cmake_minimum_required(VERSION 3.0...3.27)
project(
  "Suffix Stack"
  VERSION 0.1.0
  LANGUAGES CXX
)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

add_library(suffix_stack suffstack.cc suffstack.hpp)

add_executable(tests tests.cc)
target_link_libraries(tests suffix_stack)

set_target_properties(suffix_stack tests PROPERTIES
  CXX_STANDARD 20
)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

function(add_cxx_args target)
  if (MSVC)
    target_compile_options(${target} PRIVATE /w4)
  else()
    target_compile_options(${target} PRIVATE -Wall -Wextra -pedantic -fno-omit-frame-pointer)
  endif()
endfunction()

add_cxx_args(suffix_stack)
add_cxx_args(tests)
