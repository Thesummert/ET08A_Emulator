cmake_minimum_required(VERSION 3.21)
project(ET08A_Emulator VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
option(ET08_BUILD_APP "Build the Slint desktop application" ON)
option(ET08_FETCH_SLINT "Download Slint when no installed package is found" ON)
include(CTest)
find_package(Threads REQUIRED)

add_library(et08_core src/protocol.cpp src/sender.cpp)
target_include_directories(et08_core PUBLIC include)
target_link_libraries(et08_core PUBLIC Threads::Threads)
if(MSVC)
    target_compile_options(et08_core PUBLIC /utf-8 PRIVATE /W4)
else()
    target_compile_options(et08_core PRIVATE -Wall -Wextra -Wpedantic)
endif()

add_executable(et08a_frame tools/frame_dump.cpp)
target_link_libraries(et08a_frame PRIVATE et08_core)

if(ET08_BUILD_APP)
    # Works with system packages, a local prefix, and the vcpkg toolchain.
    find_path(SERIALPORT_INCLUDE_DIR NAMES libserialport.h)
    find_library(SERIALPORT_LIBRARY NAMES serialport libserialport)
    if(NOT SERIALPORT_INCLUDE_DIR OR NOT SERIALPORT_LIBRARY)
        message(FATAL_ERROR "libserialport is missing. Install its development package or use vcpkg; see README.md. Use -DET08_BUILD_APP=OFF to build core tests only.")
    endif()
    add_library(SerialPort::SerialPort UNKNOWN IMPORTED)
    set_target_properties(SerialPort::SerialPort PROPERTIES
        IMPORTED_LOCATION "${SERIALPORT_LIBRARY}"
        INTERFACE_INCLUDE_DIRECTORIES "${SERIALPORT_INCLUDE_DIR}")

    find_package(Slint 1.18 QUIET)
    if(NOT Slint_FOUND)
        if(NOT ET08_FETCH_SLINT)
            message(FATAL_ERROR "Install Slint >= 1.18 and set CMAKE_PREFIX_PATH, or enable ET08_FETCH_SLINT")
        endif()
        include(FetchContent)
        FetchContent_Declare(Slint
            GIT_REPOSITORY https://github.com/slint-ui/slint.git
            GIT_TAG v1.18.1
            GIT_SHALLOW TRUE
            SOURCE_SUBDIR api/cpp)
        FetchContent_MakeAvailable(Slint)
    endif()

    add_executable(et08a_emulator src/main.cpp src/serial_port.cpp)
    target_link_libraries(et08a_emulator PRIVATE et08_core Slint::Slint SerialPort::SerialPort)
    slint_target_sources(et08a_emulator ui/app.slint)
    if(WIN32)
        # Includes Slint's DLL when using a shared Slint distribution.
        add_custom_command(TARGET et08a_emulator POST_BUILD
            COMMAND ${CMAKE_COMMAND}
                "-DDLL_LIST=$<JOIN:$<TARGET_RUNTIME_DLLS:et08a_emulator>,|>"
                "-DDESTINATION=$<TARGET_FILE_DIR:et08a_emulator>"
                -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/copy_runtime.cmake")
        get_filename_component(SERIALPORT_PREFIX "${SERIALPORT_INCLUDE_DIR}" DIRECTORY)
        find_file(SERIALPORT_RUNTIME_DLL NAMES libserialport-0.dll serialport.dll libserialport.dll
            HINTS "${SERIALPORT_PREFIX}/bin")
        if(SERIALPORT_RUNTIME_DLL)
            add_custom_command(TARGET et08a_emulator POST_BUILD
                COMMAND ${CMAKE_COMMAND} -E copy_if_different
                    "${SERIALPORT_RUNTIME_DLL}" $<TARGET_FILE_DIR:et08a_emulator>)
        endif()
    endif()
endif()

if(BUILD_TESTING)
    add_executable(protocol_test tests/protocol_test.cpp)
    target_link_libraries(protocol_test PRIVATE et08_core)
    add_test(NAME protocol COMMAND protocol_test)
    add_executable(sender_test tests/sender_test.cpp)
    target_link_libraries(sender_test PRIVATE et08_core)
    add_test(NAME sender COMMAND sender_test)
endif()
