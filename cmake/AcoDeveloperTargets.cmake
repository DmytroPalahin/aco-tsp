# Optional targets for people working on the project itself:
#
#   format        rewrite the sources with clang-format
#   format-check  fail if any source is not formatted
#   docs          build the API reference with Doxygen
#   report        typeset report/report.tex with latexmk
#   coverage      run the tests and report the lines they executed
#
# Each of the first four is only defined when the tool it needs is installed;
# "coverage" belongs to builds configured with ACO_ENABLE_COVERAGE.

file(
  GLOB_RECURSE
  ACO_FORMAT_SOURCES
  CONFIGURE_DEPENDS
  "${PROJECT_SOURCE_DIR}/include/*.h"
  "${PROJECT_SOURCE_DIR}/src/*.[ch]"
  "${PROJECT_SOURCE_DIR}/tests/*.[ch]"
  "${PROJECT_SOURCE_DIR}/examples/*.c"
  "${PROJECT_SOURCE_DIR}/fuzz/*.c")

find_program(ACO_CLANG_FORMAT NAMES clang-format)
if(ACO_CLANG_FORMAT)
  add_custom_target(
    format
    COMMAND ${ACO_CLANG_FORMAT} -i ${ACO_FORMAT_SOURCES}
    COMMENT "Formatting the sources"
    VERBATIM)
  add_custom_target(
    format-check
    COMMAND ${ACO_CLANG_FORMAT} --dry-run --Werror ${ACO_FORMAT_SOURCES}
    COMMENT "Checking the formatting of the sources"
    VERBATIM)
endif()

find_package(Doxygen QUIET)
if(DOXYGEN_FOUND)
  configure_file("${PROJECT_SOURCE_DIR}/docs/Doxyfile.in" "${PROJECT_BINARY_DIR}/Doxyfile" @ONLY)
  add_custom_target(
    docs
    COMMAND Doxygen::doxygen "${PROJECT_BINARY_DIR}/Doxyfile"
    WORKING_DIRECTORY "${PROJECT_SOURCE_DIR}"
    COMMENT "Generating the API reference in ${PROJECT_BINARY_DIR}/docs/html"
    VERBATIM)
endif()

find_program(ACO_LATEXMK NAMES latexmk)
if(ACO_LATEXMK)
  add_custom_target(
    report
    COMMAND ${ACO_LATEXMK} -pdf -interaction=nonstopmode -halt-on-error
            "-output-directory=${PROJECT_BINARY_DIR}/report" report.tex
    WORKING_DIRECTORY "${PROJECT_SOURCE_DIR}/report"
    COMMENT "Typesetting the report in ${PROJECT_BINARY_DIR}/report"
    VERBATIM)
endif()

if(ACO_ENABLE_COVERAGE AND ACO_BUILD_TESTS)
  # The profile format changes between compiler releases, so the tools have
  # to be the ones that came with the compiler: PATH may hold other versions.
  set(coverage_tools "")
  if(CMAKE_C_COMPILER_ID MATCHES "Clang")
    foreach(tool llvm-cov llvm-profdata)
      execute_process(
        COMMAND "${CMAKE_C_COMPILER}" -print-prog-name=${tool}
        OUTPUT_VARIABLE tool_path
        OUTPUT_STRIP_TRAILING_WHITESPACE)
      # The driver answers with the bare name when it has no such program.
      if(IS_ABSOLUTE "${tool_path}")
        string(MAKE_C_IDENTIFIER "${tool}" tool_variable)
        string(TOUPPER "${tool_variable}" tool_variable)
        list(APPEND coverage_tools "-D${tool_variable}=${tool_path}")
      endif()
    endforeach()
  else()
    # gcov is installed beside gcc under the same name: gcc-14 has gcov-14.
    get_filename_component(compiler "${CMAKE_C_COMPILER}" REALPATH)
    string(REGEX REPLACE "gcc([^/]*)$" "gcov\\1" gcov "${compiler}")
    if(NOT gcov STREQUAL compiler AND EXISTS "${gcov}")
      list(APPEND coverage_tools "-DGCOV=${gcov}")
    endif()
    find_program(ACO_GCOVR NAMES gcovr)
    if(ACO_GCOVR)
      list(APPEND coverage_tools "-DGCOVR=${ACO_GCOVR}")
    endif()
  endif()

  # Everything the tests execute: the tool, if it is built, and the test
  # programs themselves.
  get_property(
    instrumented
    DIRECTORY "${PROJECT_SOURCE_DIR}/tests"
    PROPERTY BUILDSYSTEM_TARGETS)
  if(TARGET aco-tsp)
    list(PREPEND instrumented aco-tsp)
  endif()
  list(TRANSFORM instrumented REPLACE "(.+)" "$<TARGET_FILE:\\1>" OUTPUT_VARIABLE binaries)
  list(JOIN binaries "\n" binaries)
  set(binaries_file "${PROJECT_BINARY_DIR}/coverage-binaries-$<CONFIG>.txt")
  file(
    GENERATE
    OUTPUT "${binaries_file}"
    CONTENT "${binaries}\n")

  add_custom_target(
    coverage
    COMMAND
      ${CMAKE_COMMAND} -DSOURCE_DIR=${PROJECT_SOURCE_DIR} -DBINARY_DIR=${PROJECT_BINARY_DIR}
      -DCONFIG=$<CONFIG> -DCOMPILER_ID=${CMAKE_C_COMPILER_ID} -DBINARIES_FILE=${binaries_file}
      -DMINIMUM=${ACO_COVERAGE_MINIMUM} ${coverage_tools} -P
      ${CMAKE_CURRENT_LIST_DIR}/AcoCoverageReport.cmake
    COMMENT "Measuring the test coverage"
    USES_TERMINAL VERBATIM)
  add_dependencies(coverage ${instrumented})
endif()
