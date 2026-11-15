# Fails unless the shared library exports exactly the symbols listed in EXPECTED.
foreach(var LIBRARY EXPECTED TOOL)
    if(NOT DEFINED ${var})
        message(FATAL_ERROR "check_exports.cmake needs -D${var}=...")
    endif()
endforeach()

if(MSVC)
    execute_process(COMMAND ${TOOL} /dump /exports ${LIBRARY}
        OUTPUT_VARIABLE dump RESULT_VARIABLE result)
    set(pattern "^ +[0-9]+ +[0-9A-F]+ +[0-9A-F]+ +([A-Za-z_][A-Za-z0-9_]*)")
else()
    execute_process(COMMAND ${TOOL} -D --defined-only ${LIBRARY}
        OUTPUT_VARIABLE dump RESULT_VARIABLE result)
    set(pattern "^[0-9a-fA-F]+ [TDBRVW] ([A-Za-z_][A-Za-z0-9_@.]*)")
endif()
if(NOT result EQUAL 0)
    message(FATAL_ERROR "listing exports of ${LIBRARY} failed")
endif()

string(REPLACE "\r" "" dump "${dump}")
string(REPLACE "\n" ";" lines "${dump}")
set(actual "")
foreach(line IN LISTS lines)
    if(line MATCHES "${pattern}")
        list(APPEND actual "${CMAKE_MATCH_1}")
    endif()
endforeach()
list(REMOVE_DUPLICATES actual)
list(SORT actual)

file(STRINGS ${EXPECTED} expected)
list(SORT expected)

if(NOT actual STREQUAL expected)
    message(FATAL_ERROR "exported symbols differ\n  expected: ${expected}\n  actual:   ${actual}")
endif()
message(STATUS "exports match: ${actual}")
