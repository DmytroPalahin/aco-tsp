# Unit tests: one executable per library module, each linked against the
# public API only.

function(aco_add_test name)
  add_executable(${name} ${name}.c)
  target_link_libraries(${name} PRIVATE aco::aco)
  target_include_directories(${name} PRIVATE support)
  target_compile_definitions(
    ${name} PRIVATE ACO_TEST_DATA_DIR="${PROJECT_SOURCE_DIR}/data"
                    ACO_TEST_OUTPUT_DIR="${CMAKE_CURRENT_BINARY_DIR}")
  aco_target_defaults(${name})
  add_test(NAME ${name} COMMAND ${name})
  set_tests_properties(${name} PROPERTIES LABELS unit TIMEOUT 120)
endfunction()

aco_add_test(test_rng)
aco_add_test(test_instance)
aco_add_test(test_tour)
aco_add_test(test_heuristics)
aco_add_test(test_colony)

# End-to-end tests of the command-line tool, driven by a CMake script so that
# they run unchanged on every platform.
if(ACO_BUILD_CLI)
  add_test(
    NAME test_cli
    COMMAND
      ${CMAKE_COMMAND} -DACO_TSP=$<TARGET_FILE:aco-tsp> -DDATA_DIR=${PROJECT_SOURCE_DIR}/data
      -DWORK_DIR=${CMAKE_CURRENT_BINARY_DIR}/cli -DVERSION=${PROJECT_VERSION} -P
      ${CMAKE_CURRENT_SOURCE_DIR}/cli/run.cmake)
  set_tests_properties(test_cli PROPERTIES LABELS cli TIMEOUT 120)
endif()
