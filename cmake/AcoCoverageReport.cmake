# Measures how much of the library and of the tool the test suite executes.
#
# Run as a script by the "coverage" target (see AcoDeveloperTargets.cmake),
# which passes the settings of the build tree with -D. The script runs every
# test of an instrumented build and writes, in <BINARY_DIR>/coverage-report:
#
#   summary.txt  the table of the files, which is also printed
#   html/        the sources, annotated line by line
#
# It fails if a test fails, or if fewer than MINIMUM percent of the lines
# were executed.

cmake_minimum_required(VERSION 3.21)

set(report_dir "${BINARY_DIR}/coverage-report")
file(REMOVE_RECURSE "${report_dir}")
file(MAKE_DIRECTORY "${report_dir}/html")

set(run_tests "${CMAKE_CTEST_COMMAND}" --test-dir "${BINARY_DIR}" --build-config "${CONFIG}"
              --output-on-failure --no-tests=error)

if(COMPILER_ID MATCHES "Clang")
  if(NOT LLVM_COV OR NOT LLVM_PROFDATA)
    message(FATAL_ERROR "llvm-cov and llvm-profdata were not found beside the compiler: "
                        "install the LLVM tools of the same version")
  endif()

  # One raw profile per process: the end-to-end tests start the tool many
  # times over.
  set(ENV{LLVM_PROFILE_FILE} "${report_dir}/profiles/%p-%m.profraw")
  execute_process(COMMAND ${run_tests} COMMAND_ERROR_IS_FATAL ANY)
  file(GLOB profiles "${report_dir}/profiles/*.profraw")
  set(profile "${report_dir}/coverage.profdata")
  execute_process(COMMAND "${LLVM_PROFDATA}" merge -sparse ${profiles} -o "${profile}"
                  COMMAND_ERROR_IS_FATAL ANY)

  # llvm-cov takes the first binary as it is and the others behind -object.
  # The directories at the end restrict the report to the code under test.
  file(STRINGS "${BINARIES_FILE}" binaries)
  list(JOIN binaries ";-object;" selection)
  list(APPEND selection "-instr-profile=${profile}" "${SOURCE_DIR}/src" "${SOURCE_DIR}/include")

  execute_process(COMMAND "${LLVM_COV}" report ${selection} -show-region-summary=false
                  OUTPUT_FILE "${report_dir}/summary.txt" COMMAND_ERROR_IS_FATAL ANY)
  execute_process(COMMAND "${LLVM_COV}" show ${selection} -format=html
                          "-output-dir=${report_dir}/html" COMMAND_ERROR_IS_FATAL ANY)
  execute_process(COMMAND "${LLVM_COV}" export ${selection} -summary-only
                  OUTPUT_VARIABLE totals COMMAND_ERROR_IS_FATAL ANY)
  string(JSON covered GET "${totals}" data 0 totals lines covered)
  string(JSON total GET "${totals}" data 0 totals lines count)
else()
  if(NOT GCOV OR NOT GCOVR)
    message(FATAL_ERROR "A GCC build is measured with gcovr and the gcov of the compiler; "
                        "one of them was not found")
  endif()

  # The counters accumulate from one run to the next: start from zero.
  file(GLOB_RECURSE counters "${BINARY_DIR}/*.gcda")
  if(counters)
    file(REMOVE ${counters})
  endif()
  execute_process(COMMAND ${run_tests} COMMAND_ERROR_IS_FATAL ANY)

  # A relative filter is resolved against the working directory.
  execute_process(
    COMMAND
      "${GCOVR}" --root . --filter src/ --object-directory "${BINARY_DIR}" --gcov-executable
      "${GCOV}" --txt "${report_dir}/summary.txt" --html-details "${report_dir}/html/index.html"
      --json-summary "${report_dir}/summary.json"
    WORKING_DIRECTORY "${SOURCE_DIR}" COMMAND_ERROR_IS_FATAL ANY)
  file(READ "${report_dir}/summary.json" totals)
  string(JSON covered GET "${totals}" line_covered)
  string(JSON total GET "${totals}" line_total)
endif()

execute_process(COMMAND "${CMAKE_COMMAND}" -E cat "${report_dir}/summary.txt")

if(total EQUAL 0)
  message(FATAL_ERROR "The tests left no coverage data in ${BINARY_DIR}")
endif()

# CMake computes with integers only: work in hundredths of a percent.
math(EXPR hundredths "(${covered} * 10000 + ${total} / 2) / ${total}")
math(EXPR units "${hundredths} / 100")
math(EXPR cents "${hundredths} % 100")
if(cents LESS 10)
  set(cents "0${cents}")
endif()
set(percent "${units}.${cents}")

message(STATUS "Line coverage: ${percent} % (${covered} of ${total} lines)")
message(STATUS "Annotated sources: ${report_dir}/html/index.html")
if(percent LESS MINIMUM)
  message(FATAL_ERROR "Line coverage is ${percent} %, below the required ${MINIMUM} %")
endif()
