find_package(GTest CONFIG REQUIRED)
find_package(Threads REQUIRED)
include(GoogleTest)

if(CADPREVIEW_PYTHON_TESTS)
    # Prefer the project virtual environment created from requirements-dev.txt.
    if(NOT DEFINED Python3_EXECUTABLE)
        foreach(candidate
                ${PROJECT_SOURCE_DIR}/.venv/Scripts/python.exe
                ${PROJECT_SOURCE_DIR}/.venv/bin/python)
            if(EXISTS ${candidate})
                set(Python3_EXECUTABLE ${candidate})
                break()
            endif()
        endforeach()
    endif()
    find_package(Python3 COMPONENTS Interpreter REQUIRED)
    execute_process(COMMAND ${Python3_EXECUTABLE} -c "import pytest"
        RESULT_VARIABLE pytest_missing OUTPUT_QUIET ERROR_QUIET)
    if(pytest_missing)
        message(FATAL_ERROR
            "pytest not found for ${Python3_EXECUTABLE}. Create the dev environment with "
            "'python -m venv .venv' and 'pip install -r requirements-dev.txt', "
            "or configure with -DCADPREVIEW_PYTHON_TESTS=OFF.")
    endif()
    set(pytest_command ${Python3_EXECUTABLE} -m pytest -q)
endif()

add_library(cadpreview_test_support STATIC
    support/png_decode.cpp
    support/temp_dir.cpp)
target_include_directories(cadpreview_test_support PUBLIC support)
target_link_libraries(cadpreview_test_support PUBLIC PNG::PNG)
cadpreview_configure_target(cadpreview_test_support)

add_executable(cadpreview_unit_tests
    unit/detect_test.cpp
    unit/ladder_test.cpp
    unit/log_test.cpp
    unit/pipeline_test.cpp
    unit/placeholder_test.cpp
    unit/render_test.cpp
    unit/source_test.cpp
    unit/utf8_test.cpp)
target_link_libraries(cadpreview_unit_tests PRIVATE
    cadpreview_core cadpreview_test_support GTest::gtest GTest::gtest_main)
cadpreview_configure_target(cadpreview_unit_tests)
gtest_discover_tests(cadpreview_unit_tests TEST_PREFIX "unit." DISCOVERY_MODE PRE_TEST)

add_executable(cadpreview_api_tests
    api/arguments_test.cpp
    api/misc_test.cpp
    api/render_test.cpp
    api/structs_test.cpp
    api/threads_test.cpp)
target_link_libraries(cadpreview_api_tests PRIVATE
    cadpreview cadpreview_test_support GTest::gtest GTest::gtest_main Threads::Threads)
cadpreview_configure_target(cadpreview_api_tests)
gtest_discover_tests(cadpreview_api_tests TEST_PREFIX "api." DISCOVERY_MODE PRE_TEST)

add_executable(cadpreview_c_api_check abi/header_c11.c)
target_link_libraries(cadpreview_c_api_check PRIVATE cadpreview)
cadpreview_configure_target(cadpreview_c_api_check)
add_test(NAME abi.c_header COMMAND cadpreview_c_api_check)

if(NOT CADPREVIEW_INSTRUMENTED)
    if(MSVC)
        set(export_tool ${CMAKE_LINKER})
    else()
        set(export_tool ${CMAKE_NM})
    endif()
    add_test(NAME abi.exports
        COMMAND ${CMAKE_COMMAND}
            -DLIBRARY=$<TARGET_FILE:cadpreview>
            -DEXPECTED=${CMAKE_CURRENT_SOURCE_DIR}/abi/exports.txt
            -DTOOL=${export_tool}
            -DMSVC=${MSVC}
            -P ${CMAKE_CURRENT_SOURCE_DIR}/abi/check_exports.cmake)

    set(stage_dir ${PROJECT_BINARY_DIR}/package-test/stage)
    add_test(NAME package.install
        COMMAND ${CMAKE_COMMAND} --install ${PROJECT_BINARY_DIR} --prefix ${stage_dir})
    set_tests_properties(package.install PROPERTIES FIXTURES_SETUP package)
    add_test(NAME package.consumer
        COMMAND ${CMAKE_CTEST_COMMAND}
            --build-and-test
                ${CMAKE_CURRENT_SOURCE_DIR}/package-consumer
                ${PROJECT_BINARY_DIR}/package-test/consumer
            --build-generator ${CMAKE_GENERATOR}
            --build-options
                -DCMAKE_BUILD_TYPE=${CMAKE_BUILD_TYPE}
                -DCMAKE_C_COMPILER=${CMAKE_C_COMPILER}
                -DCMAKE_PREFIX_PATH=${stage_dir}
            --test-command consumer)
    set_tests_properties(package.consumer PROPERTIES FIXTURES_REQUIRED package)
    if(TARGET cadpreview_cli)
        # Runs the installed CLI from the stage, which checks that it finds the installed library.
        add_test(NAME package.cli
            COMMAND ${stage_dir}/${CMAKE_INSTALL_BINDIR}/cadpreview${CMAKE_EXECUTABLE_SUFFIX} --version)
        set_tests_properties(package.cli PROPERTIES
            FIXTURES_REQUIRED package
            PASS_REGULAR_EXPRESSION "^cadpreview ${PROJECT_VERSION}")
    endif()
endif()

if(CADPREVIEW_PYTHON_TESTS)
    if(TARGET cadpreview_cli)
        add_test(NAME cli.behaviour
            COMMAND ${pytest_command} ${CMAKE_CURRENT_SOURCE_DIR}/cli/test_cli.py)
        set_tests_properties(cli.behaviour PROPERTIES
            ENVIRONMENT "CADPREVIEW_CLI=$<TARGET_FILE:cadpreview_cli>")
    endif()
    add_test(NAME tools.corpus
        COMMAND ${pytest_command} ${PROJECT_SOURCE_DIR}/tools/tests)
    add_test(NAME corpus.validate
        COMMAND ${Python3_EXECUTABLE} ${PROJECT_SOURCE_DIR}/tools/corpus.py validate)
endif()

if(CADPREVIEW_BUILD_DOTNET_TESTS)
    find_program(DOTNET_EXECUTABLE dotnet REQUIRED)
    add_test(NAME dotnet.wrapper
        COMMAND ${DOTNET_EXECUTABLE} test
            --project ${PROJECT_SOURCE_DIR}/bindings/dotnet/tests/CadPreview.Tests/CadPreview.Tests.csproj
            --configuration Release
            --artifacts-path ${PROJECT_BINARY_DIR}/dotnet
            -p:CadPreviewNativeDir=$<TARGET_FILE_DIR:cadpreview>)
endif()
