cmake_minimum_required(VERSION 3.10.2)

project(TextEngine VERSION 1.0 LANGUAGES CXX)

set(SDL_SHARED OFF)
set(SDL_STATIC ON)
set(SDL_CAMERA OFF)
set(SDL_RENDER OFF)
set(SDL_GPU OFF)
set(SDL_AUDIO OFF)
set(SKIP_INSTALL_ALL ON)

add_subdirectory(thirdparty/libsdl)
add_subdirectory(thirdparty/freetype)
add_subdirectory(thirdparty/harfbuzz)
add_subdirectory(thirdparty/LineBreak)
add_subdirectory(thirdparty/SheenBidi)
add_subdirectory(src)