cmake_minimum_required(VERSION 3.21)

project(
  aco-tsp
  VERSION 2.0.0
  DESCRIPTION "Ant colony optimisation for the travelling salesman problem"
  HOMEPAGE_URL "https://github.com/DmytroPalahin/aco-tsp"
  LANGUAGES C)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")

# ---------------------------------------------------------------------------
# Options
# ---------------------------------------------------------------------------

option(ACO_BUILD_CLI "Build the aco-tsp command-line tool" ON)
option(ACO_BUILD_TESTS "Build the test suite" ${PROJECT_IS_TOP_LEVEL})
option(ACO_BUILD_EXAMPLES "Build the example programs" ${PROJECT_IS_TOP_LEVEL})
option(ACO_BUILD_FUZZERS "Build the libFuzzer harnesses (requires Clang)" OFF)
option(ACO_WARNINGS_AS_ERRORS "Treat compiler warnings as errors" OFF)
option(ACO_ENABLE_COVERAGE "Instrument the build to measure test coverage" OFF)
set(ACO_SANITIZERS
    ""
    CACHE STRING "Semicolon-separated sanitizers to enable, e.g. \"address;undefined\"")

if(PROJECT_IS_TOP_LEVEL AND NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
  set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
  set_property(CACHE CMAKE_BUILD_TYPE PROPERTY STRINGS Debug Release RelWithDebInfo MinSizeRel)
endif()

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)
set(CMAKE_WINDOWS_EXPORT_ALL_SYMBOLS ON)

include(GNUInstallDirs)
include(CMakePackageConfigHelpers)
include(AcoTargetDefaults)

# ---------------------------------------------------------------------------
# Library
# ---------------------------------------------------------------------------

set(ACO_GENERATED_INCLUDE_DIR "${CMAKE_CURRENT_BINARY_DIR}/generated/include")
configure_file(include/aco/version.h.in "${ACO_GENERATED_INCLUDE_DIR}/aco/version.h" @ONLY)

add_library(
  aco
  src/colony.c
  src/exact.c
  src/heuristics.c
  src/instance.c
  src/instance_parse.c
  src/instance_write.c
  src/rng.c
  src/status.c
  src/tour.c)
add_library(aco::aco ALIAS aco)

target_include_directories(
  aco
  PUBLIC $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
         $<BUILD_INTERFACE:${ACO_GENERATED_INCLUDE_DIR}>
         $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
  PRIVATE src)
set_target_properties(aco PROPERTIES VERSION ${PROJECT_VERSION} SOVERSION ${PROJECT_VERSION_MAJOR})
if(UNIX)
  target_link_libraries(aco PUBLIC m)
endif()
aco_target_defaults(aco)

# ---------------------------------------------------------------------------
# Command-line tool
# ---------------------------------------------------------------------------

if(ACO_BUILD_CLI)
  add_executable(aco-tsp src/cli/args.c src/cli/generate.c src/cli/main.c src/cli/solve.c)
  target_link_libraries(aco-tsp PRIVATE aco::aco)
  aco_target_defaults(aco-tsp)
endif()

# ---------------------------------------------------------------------------
# Tests, examples, fuzzers
# ---------------------------------------------------------------------------

if(ACO_BUILD_TESTS)
  enable_testing()
  add_subdirectory(tests)
endif()

if(ACO_BUILD_EXAMPLES)
  add_subdirectory(examples)
endif()

if(ACO_BUILD_FUZZERS)
  add_subdirectory(fuzz)
endif()

# ---------------------------------------------------------------------------
# Installation: library, headers, CMake package, tool
# ---------------------------------------------------------------------------

set(ACO_INSTALL_CMAKEDIR "${CMAKE_INSTALL_LIBDIR}/cmake/aco")

install(
  TARGETS aco
  EXPORT aco-targets
  ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
  LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
  RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
  INCLUDES
  DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})
install(
  DIRECTORY include/aco
  DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
  FILES_MATCHING
  PATTERN "*.h")
install(FILES "${ACO_GENERATED_INCLUDE_DIR}/aco/version.h"
        DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/aco)
install(
  EXPORT aco-targets
  NAMESPACE aco::
  DESTINATION ${ACO_INSTALL_CMAKEDIR})

configure_package_config_file(
  cmake/aco-config.cmake.in "${CMAKE_CURRENT_BINARY_DIR}/aco-config.cmake"
  INSTALL_DESTINATION ${ACO_INSTALL_CMAKEDIR})
write_basic_package_version_file(
  "${CMAKE_CURRENT_BINARY_DIR}/aco-config-version.cmake" COMPATIBILITY SameMajorVersion)
install(FILES "${CMAKE_CURRENT_BINARY_DIR}/aco-config.cmake"
              "${CMAKE_CURRENT_BINARY_DIR}/aco-config-version.cmake"
        DESTINATION ${ACO_INSTALL_CMAKEDIR})

if(ACO_BUILD_CLI)
  install(TARGETS aco-tsp RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
endif()
install(FILES LICENSE README.md CHANGELOG.md DESTINATION ${CMAKE_INSTALL_DOCDIR})

# ---------------------------------------------------------------------------
# Developer conveniences (only when this is the top-level project)
# ---------------------------------------------------------------------------

if(PROJECT_IS_TOP_LEVEL)
  include(AcoDeveloperTargets)

  set(CPACK_PACKAGE_NAME "aco-tsp")
  set(CPACK_PACKAGE_VENDOR "Dmytro Palahin")
  set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "${PROJECT_DESCRIPTION}")
  set(CPACK_RESOURCE_FILE_LICENSE "${CMAKE_CURRENT_SOURCE_DIR}/LICENSE")
  set(CPACK_RESOURCE_FILE_README "${CMAKE_CURRENT_SOURCE_DIR}/README.md")
  set(CPACK_SOURCE_IGNORE_FILES "/\\\\.git/" "/build/" "/archive/")
  string(TOLOWER "${CMAKE_SYSTEM_NAME}-${CMAKE_SYSTEM_PROCESSOR}" ACO_PLATFORM)
  set(CPACK_PACKAGE_FILE_NAME "aco-tsp-${PROJECT_VERSION}-${ACO_PLATFORM}")
  if(WIN32)
    set(CPACK_GENERATOR ZIP)
  else()
    set(CPACK_GENERATOR TGZ)
  endif()
  include(CPack)
endif()
