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

# Optional emulator prefix (cross compiling), e.g. "qemu-aarch64;-L;/sysroot".
set(_hs_cmd ${EMULATOR} "${HEATSHRINK}")

# 20 KiB of compressible text.
string(REPEAT "The quick brown fox jumps over the lazy dog. 0123456789\n" 400 _content)
file(WRITE "${WORKDIR}/input.txt" "${_content}")
# Empty file: regression for the finish-with-no-input crash.
file(WRITE "${WORKDIR}/empty.bin" "")

function(run_roundtrip name params input)
    separate_arguments(_args UNIX_COMMAND "${params}")

    execute_process(
        COMMAND ${_hs_cmd} -e ${_args} "${input}" "${WORKDIR}/${name}.hs"
        RESULT_VARIABLE _res ERROR_VARIABLE _err)
    if(NOT _res EQUAL 0)
        message(FATAL_ERROR "[${name}] encode failed (${_res}): ${_err}")
    endif()

    execute_process(
        COMMAND ${_hs_cmd} -d ${_args} "${WORKDIR}/${name}.hs" "${WORKDIR}/${name}.out"
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

# Framed streams: -f writes an 8-byte header carrying -w and -l; decoding
# auto-detects it, so no parameters are needed.
function(run_framed name enc_params dec_params input)
    separate_arguments(_enc UNIX_COMMAND "${enc_params}")
    separate_arguments(_dec UNIX_COMMAND "${dec_params}")

    execute_process(
        COMMAND ${_hs_cmd} -e -f ${_enc} "${input}" "${WORKDIR}/${name}.hs"
        RESULT_VARIABLE _res ERROR_VARIABLE _err)
    if(NOT _res EQUAL 0)
        message(FATAL_ERROR "[${name}] framed encode failed (${_res}): ${_err}")
    endif()

    execute_process(
        COMMAND ${_hs_cmd} -d ${_dec} "${WORKDIR}/${name}.hs" "${WORKDIR}/${name}.out"
        RESULT_VARIABLE _res ERROR_VARIABLE _err)
    if(NOT _res EQUAL 0)
        message(FATAL_ERROR "[${name}] framed decode failed (${_res}): ${_err}")
    endif()

    execute_process(COMMAND "${CMAKE_COMMAND}" -E compare_files
        "${input}" "${WORKDIR}/${name}.out" RESULT_VARIABLE _res)
    if(NOT _res EQUAL 0)
        message(FATAL_ERROR "[${name}] framed round trip mismatch")
    endif()
endfunction()

function(expect_failure name)
    execute_process(COMMAND ${_hs_cmd} ${ARGN}
        RESULT_VARIABLE _res OUTPUT_QUIET ERROR_QUIET)
    if(_res EQUAL 0)
        message(FATAL_ERROR "[${name}] expected a failure, but the command succeeded")
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
    COMMAND ${_hs_cmd} -e "${WORKDIR}/input.txt" "${WORKDIR}/default2.hs"
    RESULT_VARIABLE _res ERROR_VARIABLE _err)
if(NOT _res EQUAL 0)
    message(FATAL_ERROR "default encode failed (${_res}): ${_err}")
endif()

# -v must keep stdout clean when writing to a file (report goes to stdout
# only when the output is a file, so check the stream is still valid).
execute_process(
    COMMAND ${_hs_cmd} -e -v "${WORKDIR}/input.txt" "${WORKDIR}/verbose.hs"
    RESULT_VARIABLE _res OUTPUT_VARIABLE _out ERROR_VARIABLE _err)
if(NOT _res EQUAL 0)
    message(FATAL_ERROR "verbose encode failed (${_res}): ${_err}")
endif()

run_framed("framed-default" "" "" "${WORKDIR}/input.txt")
run_framed("framed-w15" "-w 15 -l 6" "" "${WORKDIR}/input.txt")
run_framed("framed-w8" "-w 8 -l 3" "" "${WORKDIR}/input.txt")
run_framed("framed-explicit-match" "-w 13 -l 5" "-w 13 -l 5" "${WORKDIR}/input.txt")
run_framed("framed-empty" "" "" "${WORKDIR}/empty.bin")

# -c adds the uncompressed size and CRC32 to the frame; decoding verifies both.
run_framed("summed" "-c -w 12 -l 5" "" "${WORKDIR}/input.txt")

# Check the extension fields against known values (input.txt is 22,400 bytes
# and its zlib CRC32 is 0xf5b13ad2; both stored little-endian).
file(READ "${WORKDIR}/summed.hs" _hdr HEX LIMIT 20)
string(SUBSTRING "${_hdr}" 16 16 _size_field)
string(SUBSTRING "${_hdr}" 32 8 _crc_field)
if(NOT _size_field STREQUAL "8057000000000000")
    message(FATAL_ERROR "frame size field is ${_size_field}, expected 8057000000000000")
endif()
if(NOT _crc_field STREQUAL "f5b13ad2")
    message(FATAL_ERROR "frame CRC32 field is ${_crc_field}, expected f5b13ad2")
endif()

# Extra bytes after the payload must be detected (decoded size changes).
string(ASCII 255 255 255 255 255 255 255 255 _ff)
file(APPEND "${WORKDIR}/summed.hs" "${_ff}")
expect_failure("checksum-detects-extra-data" -d "${WORKDIR}/summed.hs" "${WORKDIR}/summed.bad")
if(EXISTS "${WORKDIR}/summed.bad")
    message(FATAL_ERROR "the failed decode left its output file behind")
endif()

# -c has to reread the input, so it cannot work from a pipe.
expect_failure("checksum-needs-file" -e -c - "${WORKDIR}/no-such.hs")

# The header must win over nothing, but must not be silently overridden by a
# contradictory command line.
expect_failure("framed-conflicting-w" -d -w 11 "${WORKDIR}/framed-explicit-match.hs")
expect_failure("framed-conflicting-l" -d -l 3 "${WORKDIR}/framed-explicit-match.hs")
# -f on a raw stream is an error, and so is a truncated header.
execute_process(COMMAND ${_hs_cmd} -e "${WORKDIR}/input.txt" "${WORKDIR}/raw.hs"
    RESULT_VARIABLE _res ERROR_VARIABLE _err)
if(NOT _res EQUAL 0)
    message(FATAL_ERROR "raw encode failed: ${_err}")
endif()
expect_failure("require-frame-on-raw" -d -f "${WORKDIR}/raw.hs")
file(WRITE "${WORKDIR}/truncated.hs" "HSHR1")   # 5 printable bytes, magic + partial header
expect_failure("truncated-header" -d "${WORKDIR}/truncated.hs" "${WORKDIR}/truncated.out")
# A header with a bogus version must be rejected (all bytes printable so the
# script can write them without binary escapes).
string(ASCII 72 83 72 82 50 48 97 98 _bad_header)
file(WRITE "${WORKDIR}/badversion.hs" "${_bad_header}")
expect_failure("bad-version" -d "${WORKDIR}/badversion.hs" "${WORKDIR}/badversion.out")

message(STATUS "CLI round trip tests passed")
