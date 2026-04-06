cmake_minimum_required(VERSION 3.16)
set(PROJECT_NAME "cs3-runtime")
project(${PROJECT_NAME} LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(CMAKE_C_COMPILER MATCHES "x86_64-w64-mingw32.*")
    set(IS_MINGW TRUE)
endif()

# Define __FILE_NAME__ for GCC versions older than 12
if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU" AND CMAKE_CXX_COMPILER_VERSION VERSION_LESS 12.0)
    # This adds a definition for every source file
    set_property(SOURCE ${YOUR_SOURCE_FILES} APPEND PROPERTY COMPILE_DEFINITIONS
        "__FILE_NAME__=\"$<TARGET_FILE_NAME:$<TARGET_NAME>>\"" # Simplified version
    )
    # Better version: use a macro to strip the path
    add_compile_options("-D__FILE_NAME__=__builtin_strrchr(\"/\" __FILE__, '/') ? __builtin_strrchr(\"/\" __FILE__, '/') + 1 : __FILE__")
endif()

# Detect if we are building in the Steam Linux Runtime
if(EXISTS "/etc/os-release")
    file(READ "/etc/os-release" OS_RELEASE_CONTENT)
    if(OS_RELEASE_CONTENT MATCHES "ID=[\"']?steamrt[\"']?")
        set(BUILDING_FOR_STEAM TRUE)
        message(STATUS "Build Environment: Steam Linux Runtime Detected")
        add_compile_definitions(STEAM_BUILD=1)
    endif()
endif()


if(CMAKE_SYSTEM_NAME STREQUAL "Emscripten")
    message(STATUS "Targeting Emscripten/WebAssembly ${CMAKE_SOURCE_DIR}")
    set(CMAKE_EXECUTABLE_SUFFIX ".html")  # Optional: .html shell
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -sUSE_SDL=0 -O3 -sUSE_ZLIB=1 ")
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -static")

elseif(IS_MINGW)
    message(STATUS "Targeting MinGW/Windows")
    set(SDL3_DIR "${CMAKE_SOURCE_DIR}/external/SDL3")
    set(SDL3_DISABLE_INSTALL ON)
    set(SDL3_mixer_DISABLE_INSTALL ON)

    # build application resources (including icon)
    set(APP_ICON_RC "${CMAKE_CURRENT_SOURCE_DIR}/src/app.rc")
    set(APP_ICON_OBJ "${CMAKE_CURRENT_BINARY_DIR}/app_icon.o")
    add_custom_command(OUTPUT ${APP_ICON_OBJ}
        COMMAND ${CMAKE_RC_COMPILER} -O coff -i ${APP_ICON_RC} -o ${APP_ICON_OBJ}
        DEPENDS ${APP_ICON_RC}
        COMMENT "Compiling Windows icon resource"
    )
    add_custom_target(app_icon_res DEPENDS ${APP_ICON_OBJ})

elseif(MSVC)
    message(STATUS "Using MSVC")
    add_compile_options(/W4 /permissive-)
else()
    message(STATUS "Targeting native GCC/Linux")
    message(STATUS "Using GCC or Clang")
    #add_compile_options(-Wall -Wextra -Wpedantic)
    add_compile_options(-Wall -Wextra)
endif()

add_subdirectory(src)

# Add External
if(CMAKE_SYSTEM_NAME STREQUAL "Emscripten")
    set(SDLMIXER_MOD_XMP ON CACHE BOOL "Enable libxmp module support")
    set(SDLMIXER_VENDORED ON CACHE BOOL "Use vendored libxmp")
elseif(IS_MINGW OR BUILDING_FOR_STEAM)
    set(SDL3_mixer_DISABLE_FIND_PACKAGE_SDL3 ON)
    set(SDLMIXER_MOD_XMP ON CACHE BOOL "Enable libxmp module support")
    set(SDLMIXER_VENDORED ON CACHE BOOL "Use vendored libxmp")
    set(SDLMIXER_MOD_XMP_SHARED OFF CACHE BOOL "Build libxmp statically")
    set(SDLMIXER_BUILD_SHARED_LIBS OFF CACHE BOOL "Build SDL3_mixer statically")
    set(CMAKE_FIND_PACKAGE_PREFER_CONFIG OFF)
    set(ZLIB_BUILD_TESTING OFF CACHE BOOL "Disable zlib test targets like infcover")
    #cmake --build build/mingw --target help

    # Global toggle (affects most CMake projects)
    set(BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)

    # SDL3 specific toggles
    set(SDL_SHARED OFF CACHE BOOL "" FORCE)
    set(SDL_STATIC ON CACHE BOOL "" FORCE)

    # SDL3_mixer specific toggles
    set(SDL3MIXER_SHARED OFF CACHE BOOL "" FORCE)
    set(SDL3MIXER_STATIC ON CACHE BOOL "" FORCE)

    # ZLIB specific (if using the standard zlib source)
    set(ZLIB_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)

    # Note: Zlib usually builds both; you may need to specifically
    # link to the 'zlibstatic' target if it creates two.
    set(ZLIB_BUILD_SHARED OFF CACHE BOOL "Disable zlib shared" FORCE)
    set(ZLIB_BUILD_STATIC ON CACHE BOOL "Enable zlib static" FORCE)

    set(CMAKE_POSITION_INDEPENDENT_CODE ON)
    add_subdirectory(external/zlib zlib)
endif()

add_subdirectory(external/SDL3 SDL3)
add_subdirectory(external/SDL3_mixer SDL3_mixer)

# Your app
#add_executable(${PROJECT_NAME} ${SRC_EXPORTED_FILES})
add_executable(${PROJECT_NAME} src/main.cpp)

if(BUILDING_FOR_STEAM)
    # Add the Steam-specific source file to the target
    target_sources(${PROJECT_NAME} PRIVATE src/steam_helper.cpp)

    # Also define the macro we discussed earlier so the C++ code knows
    target_compile_definitions(${PROJECT_NAME} PRIVATE STEAM_BUILD)

    # Define path to your unzipped Steamworks SDK
    set(STEAMWORKS_SDK_PATH "${CMAKE_SOURCE_DIR}/external/steamworks/sdk")

    # Add include directory
    target_include_directories(${PROJECT_NAME} PRIVATE "${STEAMWORKS_SDK_PATH}/public")

    # Don't forget to link the actual Steam library here too
    # target_link_libraries(${PROJECT_NAME} PRIVATE ${STEAM_LIBRARY_PATH})
    # Link the Linux 64-bit library
    target_link_libraries(${PROJECT_NAME} PRIVATE
        "${STEAMWORKS_SDK_PATH}/redistributable_bin/linux64/libsteam_api.so"
    )

    #set(CMAKE_INSTALL_RPATH "$ORIGIN")
    #set(CMAKE_BUILD_WITH_INSTALL_RPATH TRUE)

endif()

#enable_testing()
#add_subdirectory(tests)

#if(EMSCRIPTEN)
if(CMAKE_SYSTEM_NAME STREQUAL "Emscripten")
    set(BPATH "${CMAKE_CURRENT_BINARY_DIR}")
    set(DATA_SRC "${CMAKE_SOURCE_DIR}/data")
    set(DATA_DST "${BPATH}/ems_data")
    # custom build steps
    add_custom_target(prepare_ems_data ALL
        COMMAND ${CMAKE_COMMAND} -E remove_directory ${DATA_DST}
        COMMAND ${CMAKE_COMMAND} -E make_directory ${DATA_DST}
        COMMAND ${CMAKE_COMMAND} -E copy_directory ${DATA_SRC} ${DATA_DST}
        COMMAND ${CMAKE_COMMAND} -E echo "Moving .ogg files from ems_data/musics to ${BPATH}"
        COMMAND ${CMAKE_COMMAND} -E copy ${DATA_DST}/musics/*.ogg ${BPATH}
        COMMAND ${CMAKE_COMMAND} -E remove ${DATA_DST}/musics/*.ogg
        COMMENT "Preparing ems_data and relocating .ogg files"
    )
    add_dependencies(${PROJECT_NAME} prepare_ems_data)

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
        #-sUSE_WEBGPU=1
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

    # link application resources
    add_dependencies(${PROJECT_NAME} app_icon_res)
    target_sources(${PROJECT_NAME} PRIVATE ${APP_ICON_OBJ})
elseif(BUILDING_FOR_STEAM)
    message(STATUS "Link Target Steam Linux")
    target_link_libraries(${PROJECT_NAME}
        PRIVATE SDL3::SDL3 SDL3_mixer::SDL3_mixer zlibstatic src_lib)
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

