# Unit tests for minimp4
# Each test_*.c file becomes its own executable and CTest target.

# Helper function: add a test executable linked to minimp4
function(minimp4_add_test name)
    add_executable(${name} ${name}.c)
    target_link_libraries(${name} PRIVATE minimp4)
    target_compile_definitions(${name} PRIVATE _FILE_OFFSET_BITS=64)
    if(UNIX)
        target_link_libraries(${name} PRIVATE m)
    endif()
    add_test(NAME ${name} COMMAND ${name} WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})
endfunction()

# Tests will be added here as they are created:
minimp4_add_test(test_mux_api)
minimp4_add_test(test_demux_api)
# minimp4_add_test(test_h26x_writer)
# minimp4_add_test(test_nal_parsing)
# minimp4_add_test(test_vector_util)
# minimp4_add_test(test_bit_reader)
# minimp4_add_test(test_edge_cases)
# minimp4_add_test(test_integration)
# minimp4_add_test(test_hevc)

# Legacy integration tests using reference vectors
add_test(NAME legacy_mux_default
    COMMAND ${CMAKE_BINARY_DIR}/minimp4_test vectors/foreman.264 ${CMAKE_BINARY_DIR}/out_default.mp4
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})

add_test(NAME legacy_mux_default_verify
    COMMAND ${CMAKE_COMMAND} -E compare_files ${CMAKE_BINARY_DIR}/out_default.mp4 vectors/out_ref.mp4
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})
set_tests_properties(legacy_mux_default_verify PROPERTIES DEPENDS legacy_mux_default)

add_test(NAME legacy_mux_sequential
    COMMAND ${CMAKE_BINARY_DIR}/minimp4_test -s vectors/foreman.264 ${CMAKE_BINARY_DIR}/out_sequential.mp4
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})

add_test(NAME legacy_mux_sequential_verify
    COMMAND ${CMAKE_COMMAND} -E compare_files ${CMAKE_BINARY_DIR}/out_sequential.mp4 vectors/out_sequential_ref.mp4
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})
set_tests_properties(legacy_mux_sequential_verify PROPERTIES DEPENDS legacy_mux_sequential)

add_test(NAME legacy_mux_fragmentation
    COMMAND ${CMAKE_BINARY_DIR}/minimp4_test -f vectors/foreman.264 ${CMAKE_BINARY_DIR}/out_fragmentation.mp4
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})

add_test(NAME legacy_mux_fragmentation_verify
    COMMAND ${CMAKE_COMMAND} -E compare_files ${CMAKE_BINARY_DIR}/out_fragmentation.mp4 vectors/out_fragmentation_ref.mp4
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})
set_tests_properties(legacy_mux_fragmentation_verify PROPERTIES DEPENDS legacy_mux_fragmentation)

add_test(NAME legacy_mux_slices
    COMMAND ${CMAKE_BINARY_DIR}/minimp4_test vectors/foreman_slices.264 ${CMAKE_BINARY_DIR}/out_slices.mp4
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})

add_test(NAME legacy_mux_slices_verify
    COMMAND ${CMAKE_COMMAND} -E compare_files ${CMAKE_BINARY_DIR}/out_slices.mp4 vectors/out_slices_ref.mp4
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})
set_tests_properties(legacy_mux_slices_verify PROPERTIES DEPENDS legacy_mux_slices)

add_test(NAME legacy_demux_default
    COMMAND ${CMAKE_BINARY_DIR}/minimp4_test -d vectors/out_ref.mp4 ${CMAKE_BINARY_DIR}/out_demux.h264
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})

add_test(NAME legacy_demux_default_verify
    COMMAND ${CMAKE_COMMAND} -E compare_files ${CMAKE_BINARY_DIR}/out_demux.h264 vectors/foreman.264
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})
set_tests_properties(legacy_demux_default_verify PROPERTIES DEPENDS legacy_demux_default)

add_test(NAME legacy_demux_sequential
    COMMAND ${CMAKE_BINARY_DIR}/minimp4_test -d vectors/out_sequential_ref.mp4 ${CMAKE_BINARY_DIR}/out_demux_seq.h264
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})

add_test(NAME legacy_demux_sequential_verify
    COMMAND ${CMAKE_COMMAND} -E compare_files ${CMAKE_BINARY_DIR}/out_demux_seq.h264 vectors/foreman.264
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})
set_tests_properties(legacy_demux_sequential_verify PROPERTIES DEPENDS legacy_demux_sequential)

add_test(NAME legacy_demux_slices
    COMMAND ${CMAKE_BINARY_DIR}/minimp4_test -d vectors/out_slices_ref.mp4 ${CMAKE_BINARY_DIR}/out_demux_slices.h264
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})

add_test(NAME legacy_demux_slices_verify
    COMMAND ${CMAKE_COMMAND} -E compare_files ${CMAKE_BINARY_DIR}/out_demux_slices.h264 vectors/foreman_slices.264
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})
set_tests_properties(legacy_demux_slices_verify PROPERTIES DEPENDS legacy_demux_slices)
