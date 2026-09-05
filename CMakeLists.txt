cmake_minimum_required(VERSION 3.20)

project(pawn_tts VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

include(FetchContent)

# -----------------------------------------------------------------------------
# 1. SA-MP Plugin SDK
# -----------------------------------------------------------------------------
set(SAMP_PLUGIN_SDK_DIR "" CACHE PATH "Path to samp-plugin-sdk checkout. If empty, uses local or fetches from GitHub.")

if(SAMP_PLUGIN_SDK_DIR)
    set(SAMP_PLUGIN_SDK_INCLUDE_DIR "${SAMP_PLUGIN_SDK_DIR}")
elseif(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/third_party/samp-plugin-sdk/amxplugin.cpp")
    set(SAMP_PLUGIN_SDK_INCLUDE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/third_party/samp-plugin-sdk")
else()
    FetchContent_Declare(
        samp-plugin-sdk
        GIT_REPOSITORY https://github.com/maddinat0r/samp-plugin-sdk.git
        GIT_TAG master
    )
    FetchContent_GetProperties(samp-plugin-sdk)
    if(NOT samp-plugin-sdk_POPULATED)
        FetchContent_Populate(samp-plugin-sdk)
    endif()
    set(SAMP_PLUGIN_SDK_INCLUDE_DIR "${samp-plugin-sdk_SOURCE_DIR}")
endif()

# -----------------------------------------------------------------------------
# 2. Legacy SA-MP Plugin Target (pawn_tts)
# -----------------------------------------------------------------------------
add_library(pawn_tts SHARED
    src/cache_manager.cpp
    src/http_server.cpp
    src/tts_engine.cpp
    src/natives.cpp
    src/plugin.cpp
    ${SAMP_PLUGIN_SDK_INCLUDE_DIR}/amxplugin.cpp
)

target_include_directories(pawn_tts PRIVATE
    src
    ${SAMP_PLUGIN_SDK_INCLUDE_DIR}
    ${SAMP_PLUGIN_SDK_INCLUDE_DIR}/amx
)

if(WIN32)
    target_compile_definitions(pawn_tts PRIVATE WIN32 _WIN32_WINNT=0x0600 HAVE_STDINT_H)
    target_link_libraries(pawn_tts PRIVATE ws2_32)
    target_sources(pawn_tts PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/exports/pawn_tts.def)
    if(MINGW)
        target_compile_options(pawn_tts PRIVATE "-includestddef.h")
        target_link_options(pawn_tts PRIVATE -static -Wl,--kill-at)
    endif()
else()
    target_compile_definitions(pawn_tts PRIVATE LINUX HAVE_STDINT_H)
    target_link_libraries(pawn_tts PRIVATE pthread)
    target_compile_options(pawn_tts PRIVATE -Wall -Wextra -Wpedantic -fvisibility=hidden)
    target_link_options(pawn_tts PRIVATE -Wl,--no-undefined)
endif()

set_target_properties(pawn_tts PROPERTIES
    PREFIX ""
    OUTPUT_NAME "pawn_tts"
    LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/plugins"
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/plugins"
    ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/plugins"
)

# -----------------------------------------------------------------------------
# 3. open.mp SDK & Native Component Target (pawn_tts_component)
# -----------------------------------------------------------------------------
set(OPENMP_SDK_DIR "" CACHE PATH "Path to open.mp-sdk checkout.")
option(PAWN_TTS_BUILD_OPENMP_COMPONENT "Build native open.mp component" ON)

if(PAWN_TTS_BUILD_OPENMP_COMPONENT)
    if(EXISTS "${OPENMP_SDK_DIR}/include/sdk.hpp")
        set(OPENMP_SDK_INCLUDE_DIR "${OPENMP_SDK_DIR}/include")
        set(OPENMP_SDK_ROOT "${OPENMP_SDK_DIR}")
    elseif(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/../open.mp-sdk/include/sdk.hpp")
        set(OPENMP_SDK_INCLUDE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../open.mp-sdk/include")
        set(OPENMP_SDK_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/../open.mp-sdk")
    else()
        FetchContent_Declare(
            openmp-sdk
            GIT_REPOSITORY https://github.com/openmultiplayer/open.mp-sdk.git
            GIT_TAG master
            GIT_SUBMODULES_RECURSE TRUE
        )
        FetchContent_GetProperties(openmp-sdk)
        if(NOT openmp-sdk_POPULATED)
            FetchContent_Populate(openmp-sdk)
        endif()
        set(OPENMP_SDK_INCLUDE_DIR "${openmp-sdk_SOURCE_DIR}/include")
        set(OPENMP_SDK_ROOT "${openmp-sdk_SOURCE_DIR}")
    endif()

    add_library(pawn_tts_component SHARED
        src/cache_manager.cpp
        src/http_server.cpp
        src/tts_engine.cpp
        src/natives.cpp
        components/openmp/component.cpp
        ${SAMP_PLUGIN_SDK_INCLUDE_DIR}/amxplugin.cpp
    )

    target_include_directories(pawn_tts_component PRIVATE
        src
        ${SAMP_PLUGIN_SDK_INCLUDE_DIR}
        ${SAMP_PLUGIN_SDK_INCLUDE_DIR}/amx
        ${OPENMP_SDK_INCLUDE_DIR}
        ${OPENMP_SDK_ROOT}/lib/glm
        ${OPENMP_SDK_ROOT}/lib/robin-hood-hashing/src/include
        ${OPENMP_SDK_ROOT}/lib/span-lite/include
        ${OPENMP_SDK_ROOT}/lib/string-view-lite/include
    )

    if(WIN32)
        target_compile_definitions(pawn_tts_component PRIVATE WIN32 _WIN32_WINNT=0x0600 HAVE_STDINT_H)
        target_include_directories(pawn_tts_component PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/components/openmp/compat)
        target_link_libraries(pawn_tts_component PRIVATE ws2_32)
        target_sources(pawn_tts_component PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/exports/pawn_tts_component.def)
        if(MINGW)
            target_compile_options(pawn_tts_component PRIVATE "-includestddef.h")
            target_link_options(pawn_tts_component PRIVATE -static -Wl,--kill-at)
        endif()
    else()
        target_compile_definitions(pawn_tts_component PRIVATE LINUX HAVE_STDINT_H)
        target_link_libraries(pawn_tts_component PRIVATE pthread)
        target_compile_options(pawn_tts_component PRIVATE -Wall -Wextra -fvisibility=hidden)
        target_link_options(pawn_tts_component PRIVATE -Wl,--no-undefined)
    endif()

    set_target_properties(pawn_tts_component PROPERTIES
        PREFIX ""
        OUTPUT_NAME "pawn_tts"
        LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/components"
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/components"
        ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/components"
    )
endif()

