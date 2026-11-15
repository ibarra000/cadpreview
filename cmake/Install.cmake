include(CMakePackageConfigHelpers)

set(CADPREVIEW_CMAKE_DIR ${CMAKE_INSTALL_LIBDIR}/cmake/cadpreview)

install(TARGETS cadpreview
    EXPORT cadpreviewTargets
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    INCLUDES DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})

install(FILES
    ${PROJECT_SOURCE_DIR}/include/cadpreview/cadpreview.h
    ${PROJECT_BINARY_DIR}/include/cadpreview/version.h
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/cadpreview)

if(MSVC)
    install(FILES $<TARGET_PDB_FILE:cadpreview> DESTINATION ${CMAKE_INSTALL_BINDIR} OPTIONAL)
endif()

if(TARGET cadpreview_cli)
    install(TARGETS cadpreview_cli RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
endif()

install(EXPORT cadpreviewTargets
    NAMESPACE cadpreview::
    DESTINATION ${CADPREVIEW_CMAKE_DIR})

configure_package_config_file(
    ${PROJECT_SOURCE_DIR}/cmake/cadpreviewConfig.cmake.in
    ${PROJECT_BINARY_DIR}/cadpreviewConfig.cmake
    INSTALL_DESTINATION ${CADPREVIEW_CMAKE_DIR})

write_basic_package_version_file(
    ${PROJECT_BINARY_DIR}/cadpreviewConfigVersion.cmake
    COMPATIBILITY SameMajorVersion)

install(FILES
    ${PROJECT_BINARY_DIR}/cadpreviewConfig.cmake
    ${PROJECT_BINARY_DIR}/cadpreviewConfigVersion.cmake
    DESTINATION ${CADPREVIEW_CMAKE_DIR})

install(FILES
    ${PROJECT_SOURCE_DIR}/LICENSE
    ${PROJECT_SOURCE_DIR}/NOTICE
    ${PROJECT_SOURCE_DIR}/THIRD_PARTY_NOTICES.md
    DESTINATION ${CMAKE_INSTALL_DATADIR}/doc/cadpreview)
