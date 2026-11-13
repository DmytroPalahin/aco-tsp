# Example programs. This directory also builds on its own against an
# installed copy of the library, which is how the packaging is tested:
#
#   cmake -S examples -B build/examples -DCMAKE_PREFIX_PATH=<install prefix>

cmake_minimum_required(VERSION 3.21)

if(NOT TARGET aco::aco)
  project(aco-examples LANGUAGES C)
  find_package(aco 2 REQUIRED)
  set(CMAKE_C_STANDARD 11)
  set(CMAKE_C_STANDARD_REQUIRED ON)
endif()

foreach(example minimal convergence)
  add_executable(example_${example} ${example}.c)
  set_target_properties(example_${example} PROPERTIES OUTPUT_NAME ${example})
  target_link_libraries(example_${example} PRIVATE aco::aco)
  if(COMMAND aco_target_defaults)
    aco_target_defaults(example_${example})
  endif()
endforeach()
