cmake_minimum_required(VERSION 3.31)
project(cadpreview_package_consumer LANGUAGES C)

find_package(cadpreview 0.1 CONFIG REQUIRED)

add_executable(consumer main.c)
target_link_libraries(consumer PRIVATE cadpreview::cadpreview)

if(WIN32)
    add_custom_command(TARGET consumer POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            $<TARGET_RUNTIME_DLLS:consumer> $<TARGET_FILE_DIR:consumer>
        COMMAND_EXPAND_LISTS)
endif()
