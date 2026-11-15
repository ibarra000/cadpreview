# Fails if the Windows DLL imports the Microsoft C/C++ runtime: it is linked statically (/MT),
# so the library and NuGet package run without the VC++ redistributable.
foreach(var LIBRARY TOOL)
    if(NOT DEFINED ${var})
        message(FATAL_ERROR "check_imports.cmake needs -D${var}=...")
    endif()
endforeach()

execute_process(COMMAND ${TOOL} /dump /dependents ${LIBRARY}
    OUTPUT_VARIABLE dump RESULT_VARIABLE result)
if(NOT result EQUAL 0)
    message(FATAL_ERROR "listing imports of ${LIBRARY} failed")
endif()
string(TOLOWER "${dump}" dump)
string(REGEX MATCHALL "(msvcp[0-9_a-z]*|vcruntime[0-9_]*|ucrtbased?|api-ms-win-crt-[a-z0-9-]*)\.dll"
    runtime "${dump}")
if(runtime)
    list(REMOVE_DUPLICATES runtime)
    message(FATAL_ERROR "the library imports the C runtime dynamically: ${runtime}")
endif()
message(STATUS "no C runtime DLL imports")
