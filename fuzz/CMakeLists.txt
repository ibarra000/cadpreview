# Each harness links the library internals statically. fuzz_c_api also compiles the C boundary
# (src/api) into the harness so the fuzzer instruments it.
function(cadpreview_add_fuzz_harness name)
    set(sources ${ARGN})
    if(CADPREVIEW_LIBFUZZER)
        add_executable(${name} ${sources})
        target_compile_options(${name} PRIVATE -fsanitize=fuzzer)
        target_link_options(${name} PRIVATE -fsanitize=fuzzer)
    else()
        # Without libFuzzer the harness replays every file in the corpus once, which keeps it
        # compiling and turns known inputs into regression tests on every platform.
        add_executable(${name} ${sources} replay_main.cpp)
        add_test(NAME fuzz.${name}.replay_corpus
            COMMAND ${name} ${PROJECT_SOURCE_DIR}/corpus)
    endif()
    target_link_libraries(${name} PRIVATE cadpreview_core)
    target_include_directories(${name} PRIVATE
        ${PROJECT_SOURCE_DIR}/include ${PROJECT_BINARY_DIR}/include)
    cadpreview_configure_target(${name})
endfunction()

if(CADPREVIEW_LIBFUZZER)
    if(NOT CMAKE_CXX_COMPILER_ID MATCHES "Clang")
        message(FATAL_ERROR "CADPREVIEW_LIBFUZZER requires clang")
    endif()
    target_compile_options(cadpreview_core PRIVATE -fsanitize=fuzzer-no-link)
endif()

cadpreview_add_fuzz_harness(fuzz_render fuzz_render.cpp)
cadpreview_add_fuzz_harness(fuzz_dxf fuzz_dxf.cpp)
cadpreview_add_fuzz_harness(fuzz_dwg_preview fuzz_dwg_preview.cpp)
cadpreview_add_fuzz_harness(fuzz_image_decode fuzz_image_decode.cpp)
cadpreview_add_fuzz_harness(fuzz_c_api fuzz_c_api.cpp ${PROJECT_SOURCE_DIR}/src/api/cadpreview_api.cpp)
target_compile_definitions(fuzz_c_api PRIVATE CADPREVIEW_BUILD)
