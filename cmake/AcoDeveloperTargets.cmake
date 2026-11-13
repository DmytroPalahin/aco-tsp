# Optional targets for people working on the project itself:
#
#   format        rewrite the sources with clang-format
#   format-check  fail if any source is not formatted
#   docs          build the API reference with Doxygen
#   report        typeset report/report.tex with latexmk
#
# Each target is only defined when the tool it needs is installed.

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
