cmake_minimum_required(VERSION 3.16)
set(PROJECT_NAME "cs3-runtime")
project(${PROJECT_NAME} LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(CMAKE_C_COMPILER MATCHES "x86_64-w64-mingw32.*")
    set(IS_MINGW TRUE)
endif()


if(EMSCRIPTEN)
    message(STATUS "Targeting Emscripten/WebAssembly ${CMAKE_SOURCE_DIR}")
    set(CMAKE_EXECUTABLE_SUFFIX ".html")  # Optional: .html shell
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -sUSE_SDL=0 -O3 -sUSE_ZLIB=1 ")
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -static")
elseif(IS_MINGW)
    message(STATUS "Targeting MinGW/Windows")
    set(SDL3_DIR "${CMAKE_SOURCE_DIR}/external/SDL3")
    set(SDL3_DISABLE_INSTALL ON)
    set(SDL3_mixer_DISABLE_INSTALL ON)
elseif(MSVC)
    message(STATUS "Using MSVC")
    add_compile_options(/W4 /permissive-)
else()
    message(STATUS "Targeting native GCC/Linux")
    message(STATUS "Using GCC or Clang")
    add_compile_options(-Wall -Wextra -Wpedantic)
endif()

add_subdirectory(src)

# Add External
if(EMSCRIPTEN)
    set(SDLMIXER_MOD_XMP ON CACHE BOOL "Enable libxmp module support")
    set(SDLMIXER_VENDORED ON CACHE BOOL "Use vendored libxmp")
elseif(IS_MINGW)
    set(SDL3_mixer_DISABLE_FIND_PACKAGE_SDL3 ON)
    set(SDLMIXER_MOD_XMP ON CACHE BOOL "Enable libxmp module support")
    set(SDLMIXER_VENDORED ON CACHE BOOL "Use vendored libxmp")
    set(SDLMIXER_MOD_XMP_SHARED OFF CACHE BOOL "Build libxmp statically")
    set(SDLMIXER_BUILD_SHARED_LIBS OFF CACHE BOOL "Build SDL3_mixer statically")
    set(CMAKE_FIND_PACKAGE_PREFER_CONFIG OFF)
    set(ZLIB_BUILD_TESTING OFF CACHE BOOL "Disable zlib test targets like infcover")
    add_subdirectory(external/zlib zlib)
    #cmake --build build/mingw --target help
endif()

add_subdirectory(external/SDL3 SDL3)
add_subdirectory(external/SDL3_mixer SDL3_mixer)

# Your app
#add_executable(${PROJECT_NAME} ${SRC_EXPORTED_FILES})
add_executable(${PROJECT_NAME} src/main.cpp)

#enable_testing()
#add_subdirectory(tests)


if(EMSCRIPTEN)
    message(STATUS "Link Target EMSCRIPTEN")
    target_link_options(${PROJECT_NAME} PRIVATE
        #--preload-file ../data@data
        --preload-file ems_data@data
        --shell-file ../../src/template/body.html
        -lopenal
        -lidbfs.js
        --emrun
        -O3
        -sWASM=1
        -sMODULARIZE=1
        -sASSERTIONS=1
        -sNO_DISABLE_EXCEPTION_CATCHING
        -sEXPORT_NAME="MyApp"
        -sINITIAL_MEMORY=64MB
        -sALLOW_MEMORY_GROWTH=1
        -sASYNCIFY
        -sEXPORTED_RUNTIME_METHODS=ccall,cwrap
        -sEXPORTED_FUNCTIONS=_main,_malloc,_free
    )
    target_link_libraries(${PROJECT_NAME}
        PRIVATE SDL3::SDL3 SDL3_mixer::SDL3_mixer src_lib
    )
elseif(IS_MINGW)
    message(STATUS "Link Target MinGW/Windows")
    target_link_options(${PROJECT_NAME} PRIVATE
        -Wl,-t -mwindows
        -static-libstdc++
        -static-libgcc
        -lwinpthread
        -lmingw32
        -s
    )
    target_link_libraries(${PROJECT_NAME}
        PRIVATE SDL3::SDL3 SDL3_mixer::SDL3_mixer  zlib src_lib
    )

else()
    message(STATUS "Link Target Linux")
    #find_library(XMP_LIBRARY xmp)
    #find_library(OGG_LIBRARY ogg)
    #find_library(VORBIS_LIBRARY vorbisfile)
    find_library(ZLIB_LIBRARY z)
    target_link_libraries(${PROJECT_NAME}
        PRIVATE SDL3::SDL3 SDL3_mixer::SDL3_mixer ${ZLIB_LIBRARY} src_lib
)
endif()

target_include_directories(${PROJECT_NAME} PRIVATE
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}/external/SDL3/include
    ${CMAKE_SOURCE_DIR}/external/SDL3_mixer/include
    ${CMAKE_SOURCE_DIR}/external/zlib
)

