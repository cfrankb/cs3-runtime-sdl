cmake_minimum_required(VERSION 3.16)
set(PROJECT_NAME "cs3-runtime")
project(${PROJECT_NAME} LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(MSVC)
    add_compile_options(/W4 /permissive-)
else()
    add_compile_options(-Wall -Wextra -Wpedantic)
endif()

if(EMSCRIPTEN)
    message(STATUS "Targeting Emscripten/WebAssembly ${CMAKE_SOURCE_DIR}")
    set(CMAKE_EXECUTABLE_SUFFIX ".html")  # Optional: .html shell
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -sUSE_SDL=0 -O3 -sUSE_ZLIB=1 ")
else()
    message(STATUS "Targeting native GCC/Linux")
endif()

add_subdirectory(src)

# Add External
if(EMSCRIPTEN)
    set(libxmp_INCLUDE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/external/libxmp/include")
    set(libxmp_LIBRARY "${CMAKE_CURRENT_SOURCE_DIR}/build/ems/build_libxmp/libxmp.a")
endif()

add_subdirectory(external/SDL3 SDL3)
add_subdirectory(external/SDL3_mixer SDL3_mixer)

# Your app
add_executable(${PROJECT_NAME} ${SRC_EXPORTED_FILES})

if(EMSCRIPTEN)
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
endif()

if(EMSCRIPTEN)
    message(STATUS "Looking for libxmp.a in: ${CMAKE_CURRENT_SOURCE_DIR}/build/ems/build_libxmp")
    add_library(libxmp_static STATIC IMPORTED GLOBAL)
    set_target_properties(libxmp_static PROPERTIES
        IMPORTED_LOCATION "${CMAKE_CURRENT_SOURCE_DIR}/build/ems/build_libxmp/libxmp.a"
        INTERFACE_INCLUDE_DIRECTORIES "${CMAKE_CURRENT_SOURCE_DIR}/external/libxmp/include"
    )
    target_link_libraries(${PROJECT_NAME}
        PRIVATE SDL3::SDL3 SDL3_mixer::SDL3_mixer libxmp_static src_lib
)
else()
    #find_library(XMP_LIBRARY xmp)
    #find_library(OGG_LIBRARY ogg)
    #find_library(VORBIS_LIBRARY vorbisfile)
    find_library(ZLIB_LIBRARY z)
    target_link_libraries(${PROJECT_NAME}
        PRIVATE SDL3::SDL3 SDL3_mixer::SDL3_mixer ${ZLIB_LIBRARY} src_lib
)
endif()

target_include_directories(${PROJECT_NAME} PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_CURRENT_SOURCE_DIR}/external/SDL3/include
    ${CMAKE_CURRENT_SOURCE_DIR}/external/SDL3_mixer/include
)
