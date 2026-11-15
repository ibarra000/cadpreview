# The helper must exit with status 3 and write nothing to standard output for a non-DWG file,
# and with status 2 when called without a file. see notes/dwg.md#helper-protocol
execute_process(COMMAND ${HELPER} ${INPUT} OUTPUT_VARIABLE out RESULT_VARIABLE status)
if(NOT status EQUAL 3 OR NOT out STREQUAL "")
    message(FATAL_ERROR "expected status 3 and no output, got ${status} and '${out}'")
endif()
execute_process(COMMAND ${HELPER} OUTPUT_VARIABLE out RESULT_VARIABLE status)
if(NOT status EQUAL 2)
    message(FATAL_ERROR "expected usage status 2, got ${status}")
endif()
