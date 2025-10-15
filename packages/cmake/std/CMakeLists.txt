cmake_minimum_required(VERSION 3.16)
set(PROJECT_NAME "cs3-runtime")
project(${PROJECT_NAME} LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)


# Collect .cpp files
file(GLOB CPP_SOURCES
    src/*.cpp
    src/shared/*.cpp
    src/shared/implementers/*.cpp
)

# Add External
add_subdirectory(external/SDL3 SDL3)
add_subdirectory(external/SDL3_mixer SDL3_mixer)

#find_library(XMP_LIBRARY xmp)
find_library(ZLIB_LIBRARY z)
#find_library(OGG_LIBRARY ogg)
#find_library(VORBIS_LIBRARY vorbisfile)

# Your app
add_executable(${PROJECT_NAME} ${CPP_SOURCES})

target_link_libraries(${PROJECT_NAME} 
    PRIVATE SDL3::SDL3 SDL3_mixer::SDL3_mixer
    z
)

target_include_directories(${PROJECT_NAME} PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
#    ${CMAKE_CURRENT_SOURCE_DIR}/src/runtime
#    ${CMAKE_CURRENT_SOURCE_DIR}/src/runtime/shared
)
