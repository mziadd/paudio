cmake_minimum_required(VERSION 3.16)
project(PocketAudio LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include(FetchContent)
set(USE_TLS OFF CACHE BOOL "" FORCE)
set(USE_ZLIB OFF CACHE BOOL "" FORCE)
set(BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)
FetchContent_Declare(
    ixwebsocket
    GIT_REPOSITORY https://github.com/machinezone/ixwebsocket.git
    GIT_TAG v11.4.5
)
FetchContent_MakeAvailable(ixwebsocket)

set(POCKET_AUDIO_NETWORK server/network/socket_transport.cpp)

if(APPLE)
    enable_language(OBJCXX)
    set(POCKET_AUDIO_CAPTURE
        server/capture/mac_system_capture.mm
        server/capture/system_capture.cpp
    )
    set(POCKET_AUDIO_FRAMEWORKS
        "-framework ScreenCaptureKit"
        "-framework CoreMedia"
        "-framework CoreAudio"
        "-framework AVFoundation"
        "-framework Foundation"
    )
elseif(WIN32)
    set(POCKET_AUDIO_CAPTURE
        server/capture/win_system_capture.cpp
        server/capture/system_capture.cpp
    )
    set(POCKET_AUDIO_LIBS ole32 uuid winmm propsys)
else()
    message(FATAL_ERROR "PocketAudio requires macOS or Windows")
endif()

add_executable(pocket-audio-server
    server/main.cpp
    ${POCKET_AUDIO_CAPTURE}
    ${POCKET_AUDIO_NETWORK}
)

target_include_directories(pocket-audio-server PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/server)
target_link_libraries(pocket-audio-server PRIVATE ixwebsocket)

if(APPLE)
    target_link_libraries(pocket-audio-server PRIVATE ${POCKET_AUDIO_FRAMEWORKS})
elseif(WIN32)
    target_link_libraries(pocket-audio-server PRIVATE ${POCKET_AUDIO_LIBS})
endif()
