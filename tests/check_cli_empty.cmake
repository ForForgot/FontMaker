if(NOT DEFINED CLI OR NOT DEFINED CHARSET OR NOT DEFINED OUTPUT)
    message(FATAL_ERROR "CLI, CHARSET and OUTPUT must be provided")
endif()

file(REMOVE "${OUTPUT}")
execute_process(
    COMMAND "${CLI}"
        --font "Arial"
        --size 16
        --width 16
        --height 16
        --offset-y 100
        --charset "${CHARSET}"
        --output "${OUTPUT}"
        --format bin
    RESULT_VARIABLE result
    OUTPUT_VARIABLE stdout
    ERROR_VARIABLE stderr
)

if(result EQUAL 0)
    message(FATAL_ERROR "all-empty output unexpectedly succeeded\nstdout:\n${stdout}\nstderr:\n${stderr}")
endif()
if(EXISTS "${OUTPUT}")
    message(FATAL_ERROR "all-empty guard left an output file behind")
endif()
