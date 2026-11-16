# libFuzzer harnesses. Enabled with -DACO_BUILD_FUZZERS=ON (or the "fuzz"
# preset); requires a Clang that ships libFuzzer.

if(NOT CMAKE_C_COMPILER_ID MATCHES "Clang")
  message(FATAL_ERROR "ACO_BUILD_FUZZERS requires Clang (found ${CMAKE_C_COMPILER_ID})")
endif()

# Not every Clang comes with the fuzzer runtime: Apple's does not. Find out
# now rather than when the harness fails to link.
include(CheckCSourceCompiles)
include(CMakePushCheckState)
cmake_push_check_state(RESET)
set(CMAKE_REQUIRED_LINK_OPTIONS -fsanitize=fuzzer)
check_c_source_compiles(
  "#include <stddef.h>
   #include <stdint.h>
   int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size);
   int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size)
   {
       (void)data;
       (void)size;
       return 0;
   }"
  ACO_HAVE_LIBFUZZER)
cmake_pop_check_state()
if(NOT ACO_HAVE_LIBFUZZER)
  message(
    FATAL_ERROR
      "${CMAKE_C_COMPILER} cannot link a libFuzzer harness: it has no fuzzer runtime, "
      "which is the case of Apple's Clang. Configure again with a Clang that has one; "
      "with Homebrew's LLVM, for example:\n"
      "  cmake --preset fuzz --fresh -DCMAKE_C_COMPILER=\"$(brew --prefix llvm)/bin/clang\"")
endif()

add_executable(fuzz_instance_parse fuzz_instance_parse.c)
target_link_libraries(fuzz_instance_parse PRIVATE aco::aco)
aco_target_defaults(fuzz_instance_parse)
# aco_target_defaults() already instruments every target for coverage; only
# the fuzzer runtime (and its main function) remains to be linked in.
target_link_options(fuzz_instance_parse PRIVATE -fsanitize=fuzzer)
