# Generates a C++ source that holds a file's bytes as a std::array, for resources that must ship
# inside the library (fonts). The generated file lands in the build tree.
function(cadpreview_embed_file target input symbol)
    get_filename_component(name ${input} NAME_WE)
    set(output ${CMAKE_CURRENT_BINARY_DIR}/embedded/${name}.cpp)
    add_custom_command(
        OUTPUT ${output}
        COMMAND ${CMAKE_COMMAND} -DINPUT=${input} -DOUTPUT=${output} -DSYMBOL=${symbol}
                -P ${PROJECT_SOURCE_DIR}/cmake/EmbedFileScript.cmake
        DEPENDS ${input} ${PROJECT_SOURCE_DIR}/cmake/EmbedFileScript.cmake
        COMMENT "Embedding ${name}")
    target_sources(${target} PRIVATE ${output})
endfunction()
