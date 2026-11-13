# Compiler settings shared by every target of the project.
#
# aco_target_defaults(<target>) turns on a strict warning set and applies the
# instrumentation requested through ACO_WARNINGS_AS_ERRORS, ACO_SANITIZERS,
# ACO_BUILD_FUZZERS and ACO_ENABLE_COVERAGE. All settings are PRIVATE: nothing
# leaks to consumers of the installed library.

function(aco_target_defaults target)
  if(MSVC)
    target_compile_options(${target} PRIVATE /W4 /utf-8)
    # The "secure" CRT replacements are not portable; the ISO C functions are
    # used with explicit bounds instead.
    target_compile_definitions(${target} PRIVATE _CRT_SECURE_NO_WARNINGS)
    if(ACO_WARNINGS_AS_ERRORS)
      target_compile_options(${target} PRIVATE /WX)
    endif()
  else()
    target_compile_options(
      ${target}
      PRIVATE -Wall
              -Wextra
              -Wpedantic
              -Wshadow
              -Wconversion
              -Wsign-conversion
              -Wdouble-promotion
              -Wformat=2
              -Wstrict-prototypes
              -Wmissing-prototypes
              -Wold-style-definition
              -Wcast-qual
              -Wpointer-arith
              -Wwrite-strings
              -Wundef
              -Wvla)
    if(ACO_WARNINGS_AS_ERRORS)
      target_compile_options(${target} PRIVATE -Werror)
    endif()
  endif()

  if(ACO_SANITIZERS)
    if(MSVC)
      message(FATAL_ERROR "ACO_SANITIZERS is only supported with GCC and Clang")
    endif()
    list(JOIN ACO_SANITIZERS "," sanitizers)
    target_compile_options(
      ${target} PRIVATE -fsanitize=${sanitizers} -fno-sanitize-recover=all
                        -fno-omit-frame-pointer)
    target_link_options(${target} PRIVATE -fsanitize=${sanitizers})
  endif()

  if(ACO_BUILD_FUZZERS)
    # Coverage feedback for libFuzzer; the harnesses add the fuzzer runtime
    # themselves when they link.
    target_compile_options(${target} PRIVATE -fsanitize=fuzzer-no-link)
  endif()

  if(ACO_ENABLE_COVERAGE)
    if(CMAKE_C_COMPILER_ID MATCHES "Clang")
      target_compile_options(${target} PRIVATE -fprofile-instr-generate -fcoverage-mapping)
      target_link_options(${target} PRIVATE -fprofile-instr-generate)
    elseif(CMAKE_C_COMPILER_ID STREQUAL "GNU")
      target_compile_options(${target} PRIVATE --coverage)
      target_link_options(${target} PRIVATE --coverage)
    else()
      message(FATAL_ERROR "ACO_ENABLE_COVERAGE requires GCC or Clang")
    endif()
  endif()
endfunction()
