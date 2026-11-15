add_executable(cadpreview_cli
    args.cpp
    json.cpp
    main.cpp)
if(WIN32)
    target_sources(cadpreview_cli PRIVATE cadpreview.manifest)
endif()
target_link_libraries(cadpreview_cli PRIVATE cadpreview)
set_target_properties(cadpreview_cli PROPERTIES
    OUTPUT_NAME cadpreview
    PDB_NAME cadpreview_cli
    INSTALL_RPATH "$ORIGIN/../${CMAKE_INSTALL_LIBDIR}")
cadpreview_configure_target(cadpreview_cli)
