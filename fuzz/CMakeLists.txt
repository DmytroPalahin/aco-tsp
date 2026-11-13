# libFuzzer harnesses. Enabled with -DACO_BUILD_FUZZERS=ON (or the "fuzz"
# preset); requires a Clang that ships libFuzzer.

if(NOT CMAKE_C_COMPILER_ID MATCHES "Clang")
  message(FATAL_ERROR "ACO_BUILD_FUZZERS requires Clang (found ${CMAKE_C_COMPILER_ID})")
endif()

add_executable(fuzz_instance_parse fuzz_instance_parse.c)
target_link_libraries(fuzz_instance_parse PRIVATE aco::aco)
aco_target_defaults(fuzz_instance_parse)
# aco_target_defaults() already instruments every target for coverage; only
# the fuzzer runtime (and its main function) remains to be linked in.
target_link_options(fuzz_instance_parse PRIVATE -fsanitize=fuzzer)
