cmake_minimum_required(VERSION 3.31)

project(cadpreview
    VERSION 0.2.0
    DESCRIPTION "Read-only 2D PNG previews for CAD files"
    HOMEPAGE_URL "https://github.com/ibarra000/cadpreview"
    LANGUAGES C CXX)

if(WIN32)
    enable_language(RC)
endif()

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
# No C++20 modules are used; scanning adds GCC-only flags that clang-tidy cannot parse.
set(CMAKE_CXX_SCAN_FOR_MODULES OFF)
set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

# The Microsoft C/C++ runtime is linked statically (/MT), so cadpreview.dll and the NuGet
# package need no VC++ redistributable. Only plain C types and library-owned memory cross the
# API, so a separate runtime copy inside the DLL is safe. The vcpkg triplets match (static CRT).
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${PROJECT_BINARY_DIR}/bin)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${PROJECT_BINARY_DIR}/lib)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${PROJECT_BINARY_DIR}/lib)

option(CADPREVIEW_BUILD_TESTS "Build the test suites" ${PROJECT_IS_TOP_LEVEL})
option(CADPREVIEW_BUILD_CLI "Build the cadpreview command-line tool" ON)
option(CADPREVIEW_BUILD_DOTNET_TESTS "Run the .NET wrapper tests through CTest" OFF)
option(CADPREVIEW_PYTHON_TESTS "Run the pytest suites (CLI, corpus tools) through CTest" ON)
option(CADPREVIEW_LIBFUZZER "Build libFuzzer harnesses (clang only)" OFF)
option(CADPREVIEW_COVERAGE "Instrument for gcov coverage (GCC only)" OFF)
option(CADPREVIEW_BUILD_DWG_HELPER
    "Build cadpreview-dwg, the separate GPL-3.0-or-later program that decodes DWG with LibreDWG" OFF)
set(CADPREVIEW_SANITIZE "" CACHE STRING "Sanitizers: address, address;undefined, or thread")

include(GNUInstallDirs)
include(cmake/CompilerOptions.cmake)
include(cmake/Sanitizers.cmake)

find_package(PNG REQUIRED)
include(cmake/Cairo.cmake)
include(cmake/EmbedFile.cmake)

add_subdirectory(src)

if(CADPREVIEW_BUILD_CLI)
    add_subdirectory(cli)
endif()


if(CADPREVIEW_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()

if(CADPREVIEW_BUILD_TESTS OR CADPREVIEW_LIBFUZZER)
    add_subdirectory(fuzz)
endif()

# A separate program: it links LibreDWG and nothing of the library, so no GPL code enters
# cadpreview.dll / libcadpreview.so. see notes/licenses.md
if(CADPREVIEW_BUILD_DWG_HELPER)
    add_subdirectory(helpers/dwg)
endif()

include(cmake/Install.cmake)
