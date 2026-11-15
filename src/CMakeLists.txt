configure_file(
    ${PROJECT_SOURCE_DIR}/include/cadpreview/version.h.in
    ${PROJECT_BINARY_DIR}/include/cadpreview/version.h
    @ONLY)

add_library(cadpreview_core STATIC
    common/log.cpp
    common/utf8.cpp
    containers/source.cpp
    formats/detect.cpp
    formats/dwg/preview.cpp
    formats/dxf/document.cpp
    formats/dxf/entities.cpp
    formats/dxf/text.cpp
    formats/dxf/tokenizer.cpp
    formats/format.cpp
    render/compose.cpp
    render/decode/dib.cpp
    render/decode/png_decode.cpp
    render/font5x7.cpp
    render/image.cpp
    render/png.cpp
    render/text.cpp
    render/vector/cairo_renderer.cpp
    render/vector/drawing.cpp
    render/vector/geometry.cpp
    tiers/dwg_tiers.cpp
    tiers/dxf_tiers.cpp
    tiers/ladder.cpp
    tiers/pipeline.cpp
    tiers/placeholder.cpp)
target_include_directories(cadpreview_core PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
# cairo before libpng: static archives are searched in order on Linux, and CMake drops the
# later duplicate of PNG::PNG that cadpreview_cairo lists after libcairo.a.
target_link_libraries(cadpreview_core PRIVATE cadpreview_cairo PNG::PNG)
if(CADPREVIEW_WITH_DWG)
    message(WARNING "CADPREVIEW_WITH_DWG links LibreDWG: the resulting binaries are GPL-3.0-or-later.")
    # GLOBAL: the static core passes this dependency on to tests, CLI and fuzz targets, which
    # live in other directories.
    find_package(unofficial-libredwg CONFIG REQUIRED GLOBAL)
    target_sources(cadpreview_core PRIVATE formats/dwg/libredwg_reader.cpp)
    target_link_libraries(cadpreview_core PRIVATE unofficial::libredwg::libredwg)
    target_compile_definitions(cadpreview_core PRIVATE CADPREVIEW_WITH_DWG)
endif()
cadpreview_embed_file(cadpreview_core
    ${PROJECT_SOURCE_DIR}/resources/fonts/NotoSans-CadPreview.ttf embeddedFont)
set_target_properties(cadpreview_core PROPERTIES
    CXX_VISIBILITY_PRESET hidden
    VISIBILITY_INLINES_HIDDEN ON
    POSITION_INDEPENDENT_CODE ON)
cadpreview_configure_target(cadpreview_core)

add_library(cadpreview SHARED api/cadpreview_api.cpp)
add_library(cadpreview::cadpreview ALIAS cadpreview)
target_include_directories(cadpreview PUBLIC
    $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/include>
    $<BUILD_INTERFACE:${PROJECT_BINARY_DIR}/include>
    $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>)
target_compile_definitions(cadpreview PRIVATE CADPREVIEW_BUILD)
target_link_libraries(cadpreview PRIVATE cadpreview_core)
set_target_properties(cadpreview PROPERTIES
    VERSION ${PROJECT_VERSION}
    SOVERSION ${PROJECT_VERSION_MAJOR}
    CXX_VISIBILITY_PRESET hidden
    VISIBILITY_INLINES_HIDDEN ON)
cadpreview_configure_target(cadpreview)

if(WIN32)
    configure_file(api/cadpreview.rc.in ${CMAKE_CURRENT_BINARY_DIR}/cadpreview.rc @ONLY)
    target_sources(cadpreview PRIVATE ${CMAKE_CURRENT_BINARY_DIR}/cadpreview.rc)
elseif(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    set(export_map ${CMAKE_CURRENT_SOURCE_DIR}/api/cadpreview.map)
    target_link_options(cadpreview PRIVATE
        "LINKER:--version-script=${export_map}"
        "LINKER:--exclude-libs,ALL")
    if(NOT CADPREVIEW_INSTRUMENTED)
        target_link_options(cadpreview PRIVATE "LINKER:--no-undefined")
    endif()
    set_property(TARGET cadpreview APPEND PROPERTY LINK_DEPENDS ${export_map})
endif()
