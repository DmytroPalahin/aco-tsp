# End-to-end tests of the aco-tsp command-line tool.
#
# Usage:
#   cmake -DACO_TSP=<binary> -DDATA_DIR=<repo>/data -DWORK_DIR=<scratch dir>
#         -DVERSION=<x.y.z> -P run.cmake
#
# Every check runs the binary once and compares its exit status and output
# with what is expected. All checks run; the script fails at the end if any
# of them did.

cmake_minimum_required(VERSION 3.21)

foreach(variable ACO_TSP DATA_DIR WORK_DIR VERSION)
  if(NOT DEFINED ${variable})
    message(FATAL_ERROR "${variable} is not defined")
  endif()
endforeach()

file(REMOVE_RECURSE "${WORK_DIR}")
file(MAKE_DIRECTORY "${WORK_DIR}")

set(failures 0)
set(checks 0)

function(fail name reason)
  math(EXPR count "${failures} + 1")
  set(failures ${count} PARENT_SCOPE)
  message(STATUS "FAIL  ${name}: ${reason}")
endfunction()

# check(<name> EXIT <code> [STDOUT <regex>] [STDERR <regex>] ARGS <arg>...)
#
# Runs aco-tsp with ARGS. The output is also stored in the caller's variables
# last_stdout and last_stderr.
function(check name)
  cmake_parse_arguments(PARSE_ARGV 1 T "" "EXIT;STDOUT;STDERR" "ARGS")
  math(EXPR count "${checks} + 1")
  set(checks ${count} PARENT_SCOPE)

  execute_process(
    COMMAND "${ACO_TSP}" ${T_ARGS}
    WORKING_DIRECTORY "${WORK_DIR}"
    RESULT_VARIABLE status
    OUTPUT_VARIABLE stdout
    ERROR_VARIABLE stderr)
  # Text-mode streams end lines with CR LF on Windows.
  string(REPLACE "\r\n" "\n" stdout "${stdout}")
  string(REPLACE "\r\n" "\n" stderr "${stderr}")
  set(last_stdout "${stdout}" PARENT_SCOPE)
  set(last_stderr "${stderr}" PARENT_SCOPE)

  if(NOT status STREQUAL T_EXIT)
    fail("${name}" "exit status ${status}, expected ${T_EXIT}\n${stdout}${stderr}")
  elseif(DEFINED T_STDOUT AND NOT stdout MATCHES "${T_STDOUT}")
    fail("${name}" "standard output does not match '${T_STDOUT}':\n${stdout}")
  elseif(DEFINED T_STDERR AND NOT stderr MATCHES "${T_STDERR}")
    fail("${name}" "standard error does not match '${T_STDERR}':\n${stderr}")
  else()
    message(STATUS "ok    ${name}")
    return()
  endif()
  set(failures ${failures} PARENT_SCOPE)
endfunction()

function(expect name condition_description)
  math(EXPR count "${checks} + 1")
  set(checks ${count} PARENT_SCOPE)
  if(${ARGN})
    message(STATUS "ok    ${name}")
  else()
    fail("${name}" "${condition_description}")
    set(failures ${failures} PARENT_SCOPE)
  endif()
endfunction()

# Reads a text file written by aco-tsp, with Unix line endings.
function(read_text variable path)
  file(READ "${path}" content)
  string(REPLACE "\r\n" "\n" content "${content}")
  set(${variable} "${content}" PARENT_SCOPE)
endfunction()

set(course "${DATA_DIR}/course")

# --- General ---------------------------------------------------------------

check(version EXIT 0 STDOUT "^aco-tsp ${VERSION}\n$" ARGS --version)
check(version_command EXIT 0 STDOUT "^aco-tsp ${VERSION}\n$" ARGS version)
check(help EXIT 0 STDOUT "Usage: aco-tsp <command>" ARGS --help)
check(help_solve EXIT 0 STDOUT "Usage: aco-tsp solve" ARGS help solve)
check(help_generate EXIT 0 STDOUT "Usage: aco-tsp generate" ARGS generate --help)
check(no_arguments EXIT 2 STDERR "Usage: aco-tsp")
check(unknown_command EXIT 2 STDERR "unknown command 'frobnicate'" ARGS frobnicate)

# --- solve: results --------------------------------------------------------

check(solve_quiet EXIT 0 STDOUT "^10\n$" ARGS solve "${course}/tsp010.txt" --seed 1 --quiet)
check(solve_summary EXIT 0 STDOUT "instance     tsp010 \\(10 cities\\).*length       10\n"
      ARGS solve "${course}/tsp010.txt" --seed=1)
check(solve_exact_5 EXIT 0 STDOUT "^64\n$" ARGS solve "${course}/tsp005.txt" -a exact -q)
check(solve_exact_20 EXIT 0 STDOUT "^201\n$" ARGS solve "${course}/tsp020.txt" -a exact -q)
check(solve_nearest_neighbour EXIT 0 STDOUT "^274\n$"
      ARGS solve "${course}/tsp020.txt" --algorithm nearest-neighbour -q)
check(solve_two_opt EXIT 0 STDOUT "^210\n$"
      ARGS solve "${course}/tsp020.txt" --algorithm two-opt -q)
check(solve_all_options EXIT 0 STDOUT "^[0-9]+\n$"
      ARGS solve "${course}/tsp030.txt" -q -s 3 -m 12 -i 25 --alpha 1.5 --beta 4 --rho 0.3
           --deposit 2 --tau0 0.5 --q0 0.2 --elitist 3 --local-search 2opt --patience 10)

# The same seed must give byte-identical output, the timing line aside.
check(solve_seed_a EXIT 0 ARGS solve "${course}/tsp050.txt" --seed 42 --iterations 30)
string(REGEX REPLACE "time [^\n]*\n" "" first_run "${last_stdout}")
check(solve_seed_b EXIT 0 ARGS solve "${course}/tsp050.txt" --seed 42 --iterations 30)
string(REGEX REPLACE "time [^\n]*\n" "" second_run "${last_stdout}")
expect(solve_is_deterministic "two runs with the same seed differ" first_run STREQUAL second_run)

# --- solve: errors ---------------------------------------------------------

check(solve_no_instance EXIT 2 STDERR "missing instance file" ARGS solve)
check(solve_two_instances EXIT 2 STDERR "unexpected argument" ARGS solve a.txt b.txt)
check(solve_unknown_option EXIT 2 STDERR "unknown option '--frobnicate'"
      ARGS solve "${course}/tsp010.txt" --frobnicate)
check(solve_missing_value EXIT 2 STDERR "missing value for option '--seed'"
      ARGS solve "${course}/tsp010.txt" --seed)
check(solve_bad_number EXIT 2 STDERR "invalid value 'many' for option '--ants'"
      ARGS solve "${course}/tsp010.txt" --ants many)
check(solve_bad_algorithm EXIT 2 STDERR "invalid value 'magic' for option '--algorithm'"
      ARGS solve "${course}/tsp010.txt" -a magic)
check(solve_out_of_range EXIT 2 STDERR "rho must lie in \\[0, 1\\]"
      ARGS solve "${course}/tsp010.txt" --rho 1.5)
check(solve_trace_needs_aco EXIT 2 STDERR "--trace is only available"
      ARGS solve "${course}/tsp010.txt" -a exact --trace trace.csv)
check(solve_missing_file EXIT 1 STDERR "cannot open 'no-such-file.txt'"
      ARGS solve no-such-file.txt)
check(solve_exact_too_large EXIT 1 STDERR "at most 20 cities"
      ARGS solve "${course}/tsp030.txt" -a exact)
check(solve_plot_needs_coordinates EXIT 1 STDERR "--plot needs city coordinates"
      ARGS solve "${course}/tsp010.txt" --plot tour.dat)

file(WRITE "${WORK_DIR}/truncated.txt" "4\n1 2 3\n4 5\n")
check(solve_malformed_file EXIT 1 STDERR "truncated.txt: line 4: unexpected end of input"
      ARGS solve truncated.txt)

# --- generate --------------------------------------------------------------

check(generate_to_file EXIT 0 STDERR "wrote 30 cities to cities.tsp \\(seed 5\\)"
      ARGS generate --cities 30 --seed 5 --output cities.tsp)
check(generate_again EXIT 0 ARGS generate -n 30 -s 5 -o again.tsp)
read_text(cities "${WORK_DIR}/cities.tsp")
read_text(again "${WORK_DIR}/again.tsp")
expect(generate_is_deterministic "the same seed gave two different instances"
       cities STREQUAL again)
expect(generate_writes_tsplib "the output is not a TSPLIB EUC_2D file"
       cities MATCHES "DIMENSION : 30\nEDGE_WEIGHT_TYPE : EUC_2D\nNODE_COORD_SECTION\n")

check(generate_triangular EXIT 0 STDOUT "^4\n[0-9]+ [0-9]+ [0-9]+\n[0-9]+ [0-9]+\n[0-9]+\n$"
      ARGS generate -n 4 -s 1 --extent 50 --format triangular)
check(generate_no_cities EXIT 2 STDERR "missing required option '--cities'" ARGS generate)
check(generate_too_few EXIT 2 STDERR "outside the supported range" ARGS generate -n 1)
check(generate_bad_format EXIT 2 STDERR "invalid value 'xml' for option '--format'"
      ARGS generate -n 5 -f xml)
check(generate_stray_argument EXIT 2 STDERR "unexpected argument 'extra'"
      ARGS generate -n 5 extra)

# --- generate | solve, with every output file ------------------------------

check(pipeline EXIT 0 STDOUT "local-search=2opt"
      ARGS solve cities.tsp --seed 2 --iterations 20 --local-search 2opt
           --tour best.tour --trace trace.csv --plot tour.dat)

read_text(tour "${WORK_DIR}/best.tour")
expect(tour_file_format "best.tour is not a TSPLIB tour"
       tour MATCHES "TYPE : TOUR\nDIMENSION : 30\nTOUR_SECTION\n1\n.*\n-1\nEOF\n$")

file(STRINGS "${WORK_DIR}/trace.csv" trace)
list(LENGTH trace trace_lines)
list(GET trace 0 trace_header)
expect(trace_has_one_row_per_iteration "trace.csv has ${trace_lines} lines, expected 21"
       trace_lines EQUAL 21)
expect(trace_header "unexpected header '${trace_header}'" trace_header STREQUAL
       "iteration,iteration_best,iteration_mean,iteration_worst,global_best")

file(STRINGS "${WORK_DIR}/tour.dat" plot)
list(LENGTH plot plot_lines)
list(GET plot 1 plot_first)
list(GET plot 31 plot_last)
expect(plot_has_a_closed_loop "tour.dat has ${plot_lines} lines, expected 32" plot_lines EQUAL 32)
expect(plot_returns_to_the_start "the last point is not the first one" plot_first STREQUAL
       plot_last)

# ---------------------------------------------------------------------------

if(failures GREATER 0)
  message(FATAL_ERROR "${failures} of ${checks} command-line checks failed")
endif()
message(STATUS "all ${checks} command-line checks passed")
