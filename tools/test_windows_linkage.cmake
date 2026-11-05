# Build and run installed consumers for both Windows linkage modes.
get_filename_component(source "${CMAKE_CURRENT_LIST_DIR}/.." ABSOLUTE)

function(checked)
    execute_process(COMMAND ${ARGV} RESULT_VARIABLE status)
    if(NOT status EQUAL 0)
        message(FATAL_ERROR "Command failed (${status}): ${ARGV}")
    endif()
endfunction()

foreach(shared OFF ON)
    set(output "${source}/build/windows-linkage/${IXWEBSOCKET_CXX_COMPILER}/${shared}")
    set(prefix "${output}/install")
    checked("${CMAKE_COMMAND}" -S "${source}" -B "${output}/library" -GNinja
        "-DCMAKE_CXX_COMPILER=${IXWEBSOCKET_CXX_COMPILER}"
        "-DCMAKE_C_COMPILER=${IXWEBSOCKET_C_COMPILER}"
        -DCMAKE_BUILD_TYPE=Release -DCMAKE_CXX_STANDARD_REQUIRED=ON
        "-DBUILD_SHARED_LIBS=${shared}" -DUSE_TLS=OFF -DUSE_ZLIB=OFF
        -DUSE_TEST=OFF -DUSE_WS=ON "-DCMAKE_INSTALL_PREFIX=${prefix}")
    checked("${CMAKE_COMMAND}" --build "${output}/library" --parallel 2)
    checked("${CMAKE_COMMAND}" --install "${output}/library")
    checked("${CMAKE_COMMAND}" -S "${source}/test/cmake/windows-dll"
        -B "${output}/consumer" -GNinja
        "-DCMAKE_CXX_COMPILER=${IXWEBSOCKET_CXX_COMPILER}"
        -DCMAKE_BUILD_TYPE=Release "-DCMAKE_PREFIX_PATH=${prefix}")
    checked("${CMAKE_COMMAND}" --build "${output}/consumer")
    # Keep the installed DLL discoverable for the separately built executable.
    set(saved_path "$ENV{PATH}")
    set(ENV{PATH} "${prefix}/bin;${saved_path}")
    checked("${output}/consumer/dll_consumer.exe")
    set(ENV{PATH} "${saved_path}")
endforeach()
