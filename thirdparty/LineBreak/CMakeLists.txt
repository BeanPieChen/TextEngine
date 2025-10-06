project(LineBreak LANGUAGES CXX)

add_library(LineBreak STATIC
SwapEndian.cpp
TinyInflate.cpp
ClassesTrieData.cpp
UnicodeTrie.cpp
Pairs.cpp
LineBreaker.cpp
)

target_include_directories(LineBreak PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})