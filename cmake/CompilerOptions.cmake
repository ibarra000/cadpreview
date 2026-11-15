if(MSVC)
    # CMake's defaults link Debug and RelWithDebInfo incrementally, which ASan rejects and which
    # makes the CLI and the DLL (both named cadpreview) fight over one .ilk file.
    foreach(kind EXE SHARED MODULE)
        foreach(config DEBUG RELWITHDEBINFO)
            set(var CMAKE_${kind}_LINKER_FLAGS_${config})
            string(REGEX REPLACE "/INCREMENTAL(:YES)?( |$)" "/INCREMENTAL:NO\\2" ${var} "${${var}}")
        endforeach()
    endforeach()
endif()

function(cadpreview_configure_target target)
    if(MSVC)
        target_compile_options(${target} PRIVATE
            /W4 /utf-8 /sdl /guard:cf
            $<$<COMPILE_LANGUAGE:CXX>:/permissive- /Zc:__cplusplus>)
        target_link_options(${target} PRIVATE /guard:cf)
        if(CMAKE_CXX_COMPILER_ARCHITECTURE_ID STREQUAL "x64")
            target_link_options(${target} PRIVATE /CETCOMPAT)
        endif()
    else()
        target_compile_options(${target} PRIVATE
            -Wall -Wextra -Wpedantic -Wconversion -Wsign-conversion -Wshadow
            -fstack-protector-strong)
        target_compile_definitions(${target} PRIVATE _GLIBCXX_ASSERTIONS)
        if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
            target_link_options(${target} PRIVATE
                "LINKER:-z,relro" "LINKER:-z,now" "LINKER:-z,noexecstack")
        endif()
    endif()
    cadpreview_enable_instrumentation(${target})
endfunction()
