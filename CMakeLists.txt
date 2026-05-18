cmake_minimum_required(VERSION 3.16)
project(PocketAudio LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# ixwebsocket (WebSocket server for web listener)
include(FetchContent)
set(USE_TLS OFF CACHE BOOL "" FORCE)
set(BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)
FetchContent_Declare(
    ixwebsocket
    GIT_REPOSITORY https://github.com/machinezone/ixwebsocket.git
    GIT_TAG v11.4.5
)
FetchContent_MakeAvailable(ixwebsocket)

set(POCKET_AUDIO_INCLUDE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/include)

set(POCKET_AUDIO_NETWORK
    src/network/socket_transport.cpp
)

# --- pocket-audio-server: Mac (15+) or Windows 11 ---
if(APPLE)
    enable_language(OBJCXX)
    set(POCKET_AUDIO_CAPTURE
        src/capture/mac_system_capture.mm
        src/capture/system_capture.cpp
    )
    set(POCKET_AUDIO_FRAMEWORKS
        "-framework ScreenCaptureKit"
        "-framework CoreMedia"
        "-framework CoreAudio"
        "-framework AVFoundation"
        "-framework Foundation"
    )
elseif(WIN32)
    set(POCKET_AUDIO_CAPTURE src/capture/win_system_capture.cpp src/capture/system_capture.cpp)
    set(POCKET_AUDIO_LIBS ole32 uuid winmm propsys)
else()
    message(FATAL_ERROR "PocketAudio requires macOS or Windows")
endif()

add_executable(pocket-audio-server
    apps/server/main.cpp
    ${POCKET_AUDIO_CAPTURE}
    ${POCKET_AUDIO_NETWORK}
)

target_include_directories(pocket-audio-server PRIVATE ${POCKET_AUDIO_INCLUDE_DIR})
target_link_libraries(pocket-audio-server PRIVATE ixwebsocket)

if(APPLE)
    target_link_libraries(pocket-audio-server PRIVATE ${POCKET_AUDIO_FRAMEWORKS})
elseif(WIN32)
    target_link_libraries(pocket-audio-server PRIVATE ${POCKET_AUDIO_LIBS})
endif()
