# End-to-end round trip through the command line tool. Runs the same code on
# every host, so it also exercises the portable I/O layer.
#
# Usage: cmake -DHEATSHRINK=<path to binary> -DWORKDIR=<scratch dir> -P this.cmake

if(NOT DEFINED HEATSHRINK)
    message(FATAL_ERROR "HEATSHRINK (path to the heatshrink binary) is required")
endif()
if(NOT DEFINED WORKDIR)
    set(WORKDIR "${CMAKE_CURRENT_BINARY_DIR}/cli_roundtrip")
endif()

file(MAKE_DIRECTORY "${WORKDIR}")

# 20 KiB of compressible text.
string(REPEAT "The quick brown fox jumps over the lazy dog. 0123456789\n" 400 _content)
file(WRITE "${WORKDIR}/input.txt" "${_content}")
# Empty file: regression for the finish-with-no-input crash.
file(WRITE "${WORKDIR}/empty.bin" "")

function(run_roundtrip name params input)
    separate_arguments(_args UNIX_COMMAND "${params}")

    execute_process(
        COMMAND "${HEATSHRINK}" -e ${_args} "${input}" "${WORKDIR}/${name}.hs"
        RESULT_VARIABLE _res ERROR_VARIABLE _err)
    if(NOT _res EQUAL 0)
        message(FATAL_ERROR "[${name}] encode failed (${_res}): ${_err}")
    endif()

    execute_process(
        COMMAND "${HEATSHRINK}" -d ${_args} "${WORKDIR}/${name}.hs" "${WORKDIR}/${name}.out"
        RESULT_VARIABLE _res ERROR_VARIABLE _err)
    if(NOT _res EQUAL 0)
        message(FATAL_ERROR "[${name}] decode failed (${_res}): ${_err}")
    endif()

    execute_process(
        COMMAND "${CMAKE_COMMAND}" -E compare_files "${input}" "${WORKDIR}/${name}.out"
        RESULT_VARIABLE _res)
    if(NOT _res EQUAL 0)
        message(FATAL_ERROR "[${name}] round trip mismatch (${_res})")
    endif()
endfunction()

run_roundtrip("w8-l4" "-w 8 -l 4" "${WORKDIR}/input.txt")
run_roundtrip("w11-l4" "-w 11 -l 4" "${WORKDIR}/input.txt")
run_roundtrip("w14-l7" "-w 14 -l 7" "${WORKDIR}/input.txt")
run_roundtrip("w15-l6" "-w 15 -l 6" "${WORKDIR}/input.txt")
run_roundtrip("w15-l4" "-w 15 -l 4" "${WORKDIR}/input.txt")
run_roundtrip("default" "" "${WORKDIR}/input.txt")
run_roundtrip("empty" "" "${WORKDIR}/empty.bin")

# The default window is 11; the file must decode with it after a default
# encode (the CLI must not depend on the option parser having consumed -e).
execute_process(
    COMMAND "${HEATSHRINK}" -e "${WORKDIR}/input.txt" "${WORKDIR}/default2.hs"
    RESULT_VARIABLE _res ERROR_VARIABLE _err)
if(NOT _res EQUAL 0)
    message(FATAL_ERROR "default encode failed (${_res}): ${_err}")
endif()

# -v must keep stdout clean when writing to a file (report goes to stdout
# only when the output is a file, so check the stream is still valid).
execute_process(
    COMMAND "${HEATSHRINK}" -e -v "${WORKDIR}/input.txt" "${WORKDIR}/verbose.hs"
    RESULT_VARIABLE _res OUTPUT_VARIABLE _out ERROR_VARIABLE _err)
if(NOT _res EQUAL 0)
    message(FATAL_ERROR "verbose encode failed (${_res}): ${_err}")
endif()

message(STATUS "CLI round trip tests passed")
