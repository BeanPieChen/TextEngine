set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

ADD_EXECUTABLE(TextEngineDemo
UTF8Codec.cpp
CubeAtlas.cpp
FontCollection.cpp
TextEngine.cpp
main.cpp
ContainerUtils.h 
Map.cpp
)

target_link_libraries(TextEngineDemo PRIVATE SDL3::SDL3-static)
target_link_libraries(TextEngineDemo PRIVATE freetype)
target_link_libraries(TextEngineDemo PRIVATE harfbuzz)
target_link_libraries(TextEngineDemo PRIVATE LineBreak)
target_link_libraries(TextEngineDemo PRIVATE SheenBidi)

 
install(TARGETS TextEngineDemo DESTINATION .)
install(DIRECTORY ${CMAKE_SOURCE_DIR}/fonts DESTINATION .)