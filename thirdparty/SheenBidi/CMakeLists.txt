cmake_minimum_required(VERSION 3.16)

file(STRINGS "${CMAKE_CURRENT_SOURCE_DIR}/Headers/SheenBidi/SBVersion.h" VERSION_MACRO_LINE
  REGEX "SHEENBIDI_VERSION_STRING[ \\t]+\"([^\"]+)\""
)
string(REGEX REPLACE ".*\"([^\"]+)\".*" "\\1"
  VERSION_STRING ${VERSION_MACRO_LINE}
)
project(SheenBidi
  VERSION ${VERSION_STRING}
  LANGUAGES C CXX
)

# ------------------------------------------------------------------------------
# Options
# ------------------------------------------------------------------------------

option(SB_CONFIG_UNITY "Build with a single unity source file" ON)
option(ENABLE_ASAN "Enable address sanitizer" OFF)
option(ENABLE_UBSAN "Enable undefined behavior sanitizer" OFF)

include(CTest)

# C++ is only needed for tools
set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_CXX_STANDARD_REQUIRED OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON) # for clang-tidy

if(WIN32 AND BUILD_SHARED_LIBS)
  set(BUILDING_DLL ON)
else()
  set(BUILDING_DLL OFF)
endif()

# ------------------------------------------------------------------------------
# MSVC Configuration
# ------------------------------------------------------------------------------

# https://learn.microsoft.com/en-us/cpp/build/reference/utf-8-set-source-and-executable-character-sets-to-utf-8?view=msvc-170
# https://learn.microsoft.com/en-us/cpp/build/reference/zc-cplusplus?view=msvc-170
add_compile_options("$<$<COMPILE_LANG_AND_ID:C,MSVC>:/utf-8>")
add_compile_options("$<$<COMPILE_LANG_AND_ID:CXX,MSVC>:/utf-8;/Zc:__cplusplus>")

# ------------------------------------------------------------------------------
# Sanitizer Support
# ------------------------------------------------------------------------------

function(add_sanitizers TARGET)
  if(NOT MSVC)
    if(ENABLE_ASAN)
      target_compile_options(${TARGET} PUBLIC -fsanitize=address -fsanitize-recover=address)
      target_link_libraries(${TARGET} PUBLIC -fsanitize=address -fsanitize-recover=address)
    endif()
    if(ENABLE_UBSAN)
      target_compile_options(${TARGET} PUBLIC -fsanitize=undefined)
      target_link_libraries(${TARGET} PUBLIC -fsanitize=undefined)
    endif()
  endif()
endfunction()

# ------------------------------------------------------------------------------
# SheenBidi Library
# ------------------------------------------------------------------------------

set(SHEENBIDI_HEADERS
  Headers/SheenBidi/SheenBidi.h
  Headers/SheenBidi/SBAlgorithm.h
  Headers/SheenBidi/SBAllocator.h
  Headers/SheenBidi/SBBase.h
  Headers/SheenBidi/SBBidiType.h
  Headers/SheenBidi/SBCodepoint.h
  Headers/SheenBidi/SBCodepointSequence.h
  Headers/SheenBidi/SBGeneralCategory.h
  Headers/SheenBidi/SBLine.h
  Headers/SheenBidi/SBMirrorLocator.h
  Headers/SheenBidi/SBParagraph.h
  Headers/SheenBidi/SBRun.h
  Headers/SheenBidi/SBScript.h
  Headers/SheenBidi/SBScriptLocator.h
  Headers/SheenBidi/SBVersion.h
)
file(GLOB INTERNAL_HEADERS "Source/*.h")

if(SB_CONFIG_UNITY)
  add_definitions(-DSB_CONFIG_UNITY)
  set(SHEENBIDI_SOURCES Source/SheenBidi.c)
else()
  file(GLOB SHEENBIDI_SOURCES Source/*.c)
endif()

add_library(SheenBidi
  ${SHEENBIDI_HEADERS}
  ${INTERNAL_HEADERS}
  ${SHEENBIDI_SOURCES}
)
if(BUILDING_DLL)
  target_compile_definitions(SheenBidi PRIVATE SB_CONFIG_DLL_EXPORT)
  target_compile_definitions(SheenBidi INTERFACE SB_CONFIG_DLL_IMPORT)
  set(SHEENBIDI_PKGCONFIG_CFLAGS "${SHEENBIDI_PKGCONFIG_CFLAGS} -DSB_CONFIG_DLL_IMPORT")
endif()
target_include_directories(SheenBidi
  PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/Headers>
    $<INSTALL_INTERFACE:include>
  PRIVATE
    Source
)
set_target_properties(SheenBidi PROPERTIES
  VERSION ${PROJECT_VERSION}
  SOVERSION ${PROJECT_VERSION_MAJOR}
  PUBLIC_HEADER "${SHEENBIDI_HEADERS}"
)
add_library(SheenBidi::SheenBidi ALIAS SheenBidi)
add_sanitizers(SheenBidi)