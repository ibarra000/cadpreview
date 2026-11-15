# cadpreview-dwg: decodes a DWG file with LibreDWG and writes it to standard output as ASCII DXF.
# The binary is GPL-3.0-or-later because it links LibreDWG. It links nothing from the library
# (no cairo, no cadpreview_core), and the library only starts it as a separate process.
message(STATUS "Building cadpreview-dwg: that program is GPL-3.0-or-later (LibreDWG).")

find_package(unofficial-libredwg CONFIG REQUIRED)
# LibreDWG names its target "libredwg" under MSVC and "redwg" elsewhere, and the vcpkg port
# exports it under that name.
if(TARGET unofficial::libredwg::libredwg)
    set(cadpreview_libredwg unofficial::libredwg::libredwg)
elseif(TARGET unofficial::libredwg::redwg)
    set(cadpreview_libredwg unofficial::libredwg::redwg)
else()
    message(FATAL_ERROR "the unofficial-libredwg package defines no known target")
endif()

add_executable(cadpreview_dwg_helper main.cpp translator.cpp dxf_writer.cpp)
set_target_properties(cadpreview_dwg_helper PROPERTIES OUTPUT_NAME cadpreview-dwg)
if(WIN32)
    # UTF-8 code page, so argv and LibreDWG's fopen take any path.
    target_sources(cadpreview_dwg_helper PRIVATE ${PROJECT_SOURCE_DIR}/resources/windows/utf8.manifest)
endif()
target_link_libraries(cadpreview_dwg_helper PRIVATE ${cadpreview_libredwg})
cadpreview_configure_target(cadpreview_dwg_helper)

# License boundary (see notes/licenses.md): LibreDWG (GPL) must never be linked into the
# library, and cairo (MPL-1.1, not GPL-compatible) or library code never into this program.
foreach(target cadpreview_core cadpreview)
    get_target_property(links ${target} LINK_LIBRARIES)
    if(links MATCHES "libredwg")
        message(FATAL_ERROR "${target} links LibreDWG; it must only run in cadpreview-dwg")
    endif()
endforeach()
get_target_property(helper_links cadpreview_dwg_helper LINK_LIBRARIES)
if(helper_links MATCHES "cairo|cadpreview_core|cadpreview::|PNG::|Freetype")
    message(FATAL_ERROR "cadpreview-dwg links library code: ${helper_links}")
endif()

install(TARGETS cadpreview_dwg_helper RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
set(vcpkg_share "${_VCPKG_INSTALLED_DIR}/${VCPKG_TARGET_TRIPLET}/share")
install(FILES ${vcpkg_share}/libredwg/copyright
    DESTINATION ${CMAKE_INSTALL_DATADIR}/doc/cadpreview-dwg RENAME LIBREDWG-COPYING)
install(FILES ${vcpkg_share}/jsmn/copyright
    DESTINATION ${CMAKE_INSTALL_DATADIR}/doc/cadpreview-dwg RENAME JSMN-COPYING)
install(FILES README.md DESTINATION ${CMAKE_INSTALL_DATADIR}/doc/cadpreview-dwg)

if(CADPREVIEW_BUILD_TESTS)
    add_test(NAME dwg_helper.rejects_non_dwg
        COMMAND ${CMAKE_COMMAND}
            -DHELPER=$<TARGET_FILE:cadpreview_dwg_helper>
            -DINPUT=${PROJECT_SOURCE_DIR}/corpus/synthetic/plain-text.txt
            -P ${CMAKE_CURRENT_SOURCE_DIR}/check_rejects.cmake)
endif()
