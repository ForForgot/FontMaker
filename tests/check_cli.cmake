if(NOT DEFINED CLI OR NOT DEFINED CHARSET OR NOT DEFINED OUTPUT)
    message(FATAL_ERROR "CLI, CHARSET and OUTPUT must be provided")
endif()

execute_process(
    COMMAND "${CLI}"
        --font "Arial"
        --size 16
        --width 16
        --height 16
        --charset "${CHARSET}"
        --output "${OUTPUT}"
        --format bin
    RESULT_VARIABLE result
    OUTPUT_VARIABLE stdout
    ERROR_VARIABLE stderr
)

if(NOT result EQUAL 0)
    message(FATAL_ERROR "fontmaker_cli failed (${result})\nstdout:\n${stdout}\nstderr:\n${stderr}")
endif()

file(SIZE "${OUTPUT}" actual_size)
if(NOT actual_size EQUAL 3072)
    message(FATAL_ERROR "expected 3072 bytes for 96 16x16 glyphs, got ${actual_size}")
endif()
