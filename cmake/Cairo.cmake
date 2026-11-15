# Imported target for the static cairo that vcpkg builds with meson; vcpkg ships only pkg-config
# files for it, and Windows has no pkg-config, so the libraries are located directly.
include(SelectLibraryConfigurations)

find_package(Freetype REQUIRED)
find_package(ZLIB REQUIRED)
find_package(Threads REQUIRED)

find_path(CAIRO_INCLUDE_DIR cairo.h PATH_SUFFIXES cairo REQUIRED)

function(cadpreview_find_static_library prefix)
    find_library(${prefix}_LIBRARY_RELEASE NAMES ${ARGN} NAMES_PER_DIR
        PATHS "${_VCPKG_INSTALLED_DIR}/${VCPKG_TARGET_TRIPLET}/lib" NO_DEFAULT_PATH)
    find_library(${prefix}_LIBRARY_DEBUG NAMES ${ARGN} NAMES_PER_DIR
        PATHS "${_VCPKG_INSTALLED_DIR}/${VCPKG_TARGET_TRIPLET}/debug/lib" NO_DEFAULT_PATH)
    select_library_configurations(${prefix})
    if(NOT ${prefix}_LIBRARY)
        message(FATAL_ERROR "${ARGN} not found in the vcpkg installation")
    endif()
    set(${prefix}_LIBRARIES ${${prefix}_LIBRARIES} PARENT_SCOPE)
endfunction()

cadpreview_find_static_library(CAIRO cairo)
cadpreview_find_static_library(PIXMAN pixman-1)

add_library(cadpreview_cairo INTERFACE)
target_include_directories(cadpreview_cairo SYSTEM INTERFACE ${CAIRO_INCLUDE_DIR})
target_link_libraries(cadpreview_cairo INTERFACE
    ${CAIRO_LIBRARIES} ${PIXMAN_LIBRARIES} Freetype::Freetype PNG::PNG ZLIB::ZLIB)
if(WIN32)
    cadpreview_find_static_library(PTHREADS pthreadVC3)
    target_compile_definitions(cadpreview_cairo INTERFACE CAIRO_WIN32_STATIC_BUILD)
    target_link_libraries(cadpreview_cairo INTERFACE
        ${PTHREADS_LIBRARIES} gdi32 msimg32 dwrite d2d1 windowscodecs)
else()
    target_link_libraries(cadpreview_cairo INTERFACE Threads::Threads m)
endif()
