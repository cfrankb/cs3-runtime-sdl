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

if (BUILDING_FOR_STEAM)
    add_compile_definitions(STEAM_BUILD=1)
endif()


if(CMAKE_CXX_COMPILER_ID MATCHES "Clang" AND CMAKE_CXX_COMPILER_TARGET MATCHES "msvc")
    set(IS_CROSS_MSVC TRUE)
    add_definitions(-DCOMPILING_WITH_CLANG_MSVC)
    message(STATUS "Build Context: Linux-to-Windows (Clang/MSVC ABI)")

    set(SDL_PRECOMPILED_HEADERS OFF CACHE BOOL "" FORCE)

    add_definitions(
        -D_CRT_DECLARE_NONSTDC_NAMES=1
        -D_CRT_NONSTDC_NO_DEPRECATE=1
        -D_CRT_SECURE_NO_WARNINGS=1
        -D_CRTIMP=          # Remove dllimport from CRT functions
        -D_CRTIMP2=
        -D_CRTIMP_ALTERNATIVE=
    )

    # Before add_subdirectory(external/SDL3 ...)
    add_definitions(-UHAVE_ITOA -UHAVE__ITOA)
    set(CLANG_RT "/usr/lib/llvm-19/lib/clang/19/lib/x86_64-pc-windows-msvc/clang_rt.builtins-x86_64.lib")

    # DO NOT touch CMAKE_SHARED_LINKER_FLAGS here — toolchain already set it correctly
    # Just append clang_rt without overwriting
    foreach(flag_var
        CMAKE_SHARED_LINKER_FLAGS
        CMAKE_EXE_LINKER_FLAGS
        CMAKE_MODULE_LINKER_FLAGS)
        if(NOT "${${flag_var}}" MATCHES "clang_rt")
            set(${flag_var} "${${flag_var}} ${CLANG_RT}" CACHE STRING "" FORCE)
        endif()
    endforeach()

    # Disable libm detection
    set(HAVE_LIBM OFF CACHE BOOL "" FORCE)
    set(M_LIBRARY "" CACHE FILEPATH "" FORCE)
    set(VORBIS_M_LIBRARY "" CACHE STRING "" FORCE)

    # Disable all unwanted SDL3_mixer codecs
    set(SDLMIXER_WAVPACK  OFF CACHE BOOL "" FORCE)
    set(SDLMIXER_OPUS     OFF CACHE BOOL "" FORCE)
    set(SDLMIXER_FLAC     OFF CACHE BOOL "" FORCE)
    set(SDLMIXER_MP3      OFF CACHE BOOL "" FORCE)
    set(SDLMIXER_MIDI     OFF CACHE BOOL "" FORCE)

    set(CMAKE_LINK_DEF_FILE_FLAG "/DEF:")
    add_definitions(-DNOMINMAX)
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

elseif(IS_CROSS_MSVC)
    message(STATUS "Targeting Clang/MSVC Cross-Build (Docker)")

    # llvm-rc is the Clang equivalent to MSVC's rc.exe
    set(APP_ICON_RC "${CMAKE_CURRENT_SOURCE_DIR}/src/app.rc")
    set(APP_ICON_RES "${CMAKE_CURRENT_BINARY_DIR}/app.res")

    add_custom_command(OUTPUT "${APP_ICON_RES}"
        COMMAND llvm-rc -I "${XWIN_DIR}/sdk/include/um"
                        -I "${XWIN_DIR}/sdk/include/shared"
                        /fo "${APP_ICON_RES}"
                        -- "${APP_ICON_RC}"
        DEPENDS "${APP_ICON_RC}"
        WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/src/shared"
        COMMENT "Compiling Windows resource with llvm-rc"
        VERBATIM
    )

    # In MSVC/Clang-cl, you just add the .res file to your executable sources
    list(APPEND PLATFORM_SOURCES ${APP_ICON_RES})
    set_source_files_properties("${APP_ICON_RES}" PROPERTIES EXTERNAL_OBJECT TRUE)

    # 2. SDL3 Pathing
    # Ensure SDL3 knows we are building for MSVC ABI
    set(SDL3_DIR "${CMAKE_SOURCE_DIR}/external/SDL3")
    set(SDL3_DISABLE_INSTALL ON)

    # 3. MSVC Specific Linker Flags
    # This replaces -mstackrealign and handles the Windows Subsystem
    #set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} /SUBSYSTEM:WINDOWS")
    # Add this to bypass the MSVC STL version check
    #add_definitions(-D_ALLOW_COMPILER_AND_STL_VERSION_MISMATCH)
    ####add_definitions(-DNOMINMAX)

    # Disable WavPack to stop it from trying to build a shared library [cite: 7, 8]
    set(SDL_MIXER_WAVPACK OFF CACHE BOOL "Disable WavPack" FORCE)

    # The cross (xwin) MSVC SDK does not ship gameinput.lib, so SDL's GameInput
    # backend cannot statically link the `GameInputCreate` symbol. Disable the
    # GameInput backend and fall back to SDL's XInput/HID controller support.
    set(SDL_GAMEINPUT OFF CACHE BOOL "Disable GameInput backend" FORCE)

    # 1) Make sure everything in this toolchain is static
    set(BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)
    set(SDL3MIXER_BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)
    set(SDL3MIXER_SHARED OFF CACHE BOOL "" FORCE)
    set(SDL3MIXER_STATIC ON  CACHE BOOL "" FORCE)

    # 2) Force Vorbis to be static-only in its own subproject
    set(VORBIS_BUILD_SHARED OFF CACHE BOOL "" FORCE)
    set(VORBIS_BUILD_STATIC ON  CACHE BOOL "" FORCE)

    # 3) Kill any attempt to link libm from Vorbis
    set(HAVE_LIBM OFF CACHE BOOL "" FORCE)

    # 4) Tell SDL3_mixer to use its vendored deps (so we control them)
    set(SDLMIXER_VENDORED ON CACHE BOOL "" FORCE)
    set(SDLMIXER_VORBIS   VORBISFILE CACHE STRING "" FORCE)

    add_definitions(
        -D_CRT_DECLARE_NONSTDC_NAMES=1
        -D_CRT_NONSTDC_NO_DEPRECATE=1
        -D_CRT_SECURE_NO_WARNINGS=1
    )
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
elseif(IS_MINGW OR BUILDING_FOR_STEAM OR IS_CROSS_MSVC)
    set(SDL3_mixer_DISABLE_FIND_PACKAGE_SDL3 ON)
    set(SDLMIXER_MOD_XMP ON CACHE BOOL "Enable libxmp module support")
    set(SDLMIXER_VENDORED ON CACHE BOOL "Use vendored libxmp")
    set(SDLMIXER_MOD_XMP_SHARED OFF CACHE BOOL "Build libxmp statically")
    set(SDLMIXER_BUILD_SHARED_LIBS OFF CACHE BOOL "Build SDL3_mixer statically")
    set(CMAKE_FIND_PACKAGE_PREFER_CONFIG OFF)
    set(ZLIB_BUILD_TESTING OFF CACHE BOOL "Disable zlib test targets like infcover")

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
    set(SDL3_MIXER_CMAKE_ARGS
        -DBUILD_SHARED_LIBS=OFF
        -DVORBIS_BUILD_SHARED=OFF
        -DVORBIS_BUILD_STATIC=ON
        -DM_LIBRARY=
        -DMATH_LIBRARY=
        -DVORBIS_M_LIBRARY=
    )

    set(CMAKE_POSITION_INDEPENDENT_CODE ON)
    add_subdirectory(external/zlib zlib)
endif()

add_subdirectory(external/SDL3 SDL3)
add_subdirectory(external/SDL3_mixer SDL3_mixer)

# Your app
#add_executable(${PROJECT_NAME} ${SRC_EXPORTED_FILES})
if(IS_CROSS_MSVC)
    add_executable(${PROJECT_NAME} ${PLATFORM_SOURCES} src/main.cpp)
else()
    add_executable(${PROJECT_NAME} src/main.cpp)
endif()

if(IS_CROSS_MSVC)
    # After SDL3_mixer is added, patch vorbisfile compile flags
    target_compile_options(vorbisfile PRIVATE -msse2)

    target_link_options(${PROJECT_NAME} PRIVATE
        "-Wl,/alternatename:itoa=_itoa"
        "-Wl,/alternatename:_uitoa=__uitoa"
        "-Wl,/alternatename:_ltoa=__ltoa"
        "-Wl,/alternatename:_ultoa=__ultoa"
        "-Wl,/NODEFAULTLIB:m.lib"
        #"-Wl,/ENTRY:mainCRTStartup"
        "-Wl,/SUBSYSTEM:WINDOWS"
    )
endif()


if(BUILDING_FOR_STEAM)
    # Add the Steam-specific source file to the target
    target_sources(${PROJECT_NAME} PRIVATE src/steam_helper.cpp)

    # Also define the macro we discussed earlier so the C++ code knows
    target_compile_definitions(${PROJECT_NAME} PRIVATE STEAM_BUILD)

    # Define path to your unzipped Steamworks SDK
    set(STEAMWORKS_SDK_PATH "${CMAKE_SOURCE_DIR}/external/steamworks/sdk")

    # Add include directory
    target_include_directories(${PROJECT_NAME} PRIVATE "${STEAMWORKS_SDK_PATH}/public")

    if(IS_CROSS_MSVC)
        set(STEAM_WIN_BIN "${STEAMWORKS_SDK_PATH}/redistributable_bin/win64")
        set(STEAM_LIB "${STEAM_WIN_BIN}/steam_api64.lib")
        set(STEAM_DLL "${STEAM_WIN_BIN}/steam_api64.dll")
        target_link_libraries(${PROJECT_NAME} PRIVATE ${STEAM_LIB})
    elseif(IS_MINGW)
        # Define path to Windows 64-bit SDK binaries
        #set(STEAM_WIN_BIN "${CMAKE_SOURCE_DIR}/external/steamworks/sdk/redistributable_bin/win64")
        set(STEAM_WIN_BIN "${STEAMWORKS_SDK_PATH}/redistributable_bin/win64")
        set(STEAM_DLL "${STEAM_WIN_BIN}/steam_api64.dll")
        set(STEAM_DEF "${STEAM_WIN_BIN}/steam_api64.def")
        set(STEAM_LIB_A "${CMAKE_CURRENT_BINARY_DIR}/libsteam_api64.a")

        # 2. Generate .a import library using dlltool
        add_custom_command(OUTPUT ${STEAM_LIB_A}
            COMMAND x86_64-w64-mingw32-dlltool -k -d ${STEAM_DEF} -l ${STEAM_LIB_A} -D ${STEAM_DLL}
            DEPENDS ${STEAM_DEF}
            COMMENT "Creating MinGW import library for Steam with --kill-at"
            VERBATIM
        )

        # 3. Create a Custom Target to wrap the output
        # This is what tells Make "I know how to build this file"
        add_custom_target(generate_steam_lib DEPENDS ${STEAM_LIB_A})

        # 4. Link the executable to the generated file path directly
        target_link_libraries(${PROJECT_NAME} PRIVATE ${STEAM_LIB_A})

        # 5. Force the executable to wait until the library is generated
        add_dependencies(${PROJECT_NAME} generate_steam_lib)

        # 6. Copy the DLL to the build folder [cite: 10]
        add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${STEAM_DLL}"
            "$<TARGET_FILE_DIR:${PROJECT_NAME}>"
            COMMENT "Copying steam_api64.dll to build directory"
        )

        target_compile_options(${PROJECT_NAME} PRIVATE
                -mstackrealign      # Forces 16-byte stack alignment (fixes 0xc0000025)
                -mno-ms-bitfields   # Ensures struct bitfields match MSVC layout
        )

        target_link_options(${PROJECT_NAME} PRIVATE "-Wl,--trace-symbol=SteamAPI_Init")

    else()
        # Don't forget to link the actual Steam library here too
        # target_link_libraries(${PROJECT_NAME} PRIVATE ${STEAM_LIBRARY_PATH})
        # Link the Linux 64-bit library
        target_link_libraries(${PROJECT_NAME} PRIVATE
            "${STEAMWORKS_SDK_PATH}/redistributable_bin/linux64/libsteam_api.so"
        )
    endif()

    #set(CMAKE_INSTALL_RPATH "$ORIGIN")
    #set(CMAKE_BUILD_WITH_INSTALL_RPATH TRUE)

endif()

#enable_testing()
#add_subdirectory(tests)

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
    if(BUILDING_FOR_STEAM)
        message(STATUS "Link Target Steam Linux")
        set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -mstackrealign")
        target_link_libraries(${PROJECT_NAME}
            PRIVATE SDL3::SDL3 SDL3_mixer::SDL3_mixer zlibstatic src_lib)
    else()
        target_link_libraries(${PROJECT_NAME}
            PRIVATE SDL3::SDL3 SDL3_mixer::SDL3_mixer zlibstatic src_lib ${STEAM_LIB_A})
    endif()

    # link application resources
    add_dependencies(${PROJECT_NAME} app_icon_res)
    target_sources(${PROJECT_NAME} PRIVATE ${APP_ICON_OBJ})
elseif(IS_CROSS_MSVC)
    # Tell CMake that 'm' (math library) is already handled by the CRT
    set(CMAKE_C_STANDARD_LIBRARIES "${CMAKE_C_STANDARD_LIBRARIES} msvcrt.lib")

    # Also, I noticed some libraries are missing the .lib extension in your list
    # (like -lkernel32). MSVC expects kernel32.lib.
    # To be safe, ensure the core Windows libs are definitely found:
    target_link_libraries(${PROJECT_NAME}  PRIVATE ucrt.lib msvcrt.lib)

    get_target_property(SDL_MIXER_DEPS SDL3_mixer::SDL3_mixer INTERFACE_LINK_LIBRARIES)
    message(STATUS "SDL3_mixer dependencies: ${SDL_MIXER_DEPS}")

    get_target_property(SDL_DEPS SDL3::SDL3 INTERFACE_LINK_LIBRARIES)
    message(STATUS "SDL3 dependencies: ${SDL_DEPS}")

    # This prevents 'find_library' from ever trying to find 'm'
    set(M_LIBRARY "" CACHE FILEPATH "Math library is integrated in UCRT" FORCE)
    set(MATH_LIBRARY "" CACHE FILEPATH "Math library is integrated in UCRT" FORCE)

    # 2) Force Vorbis to be static-only in its own subproject
    set(VORBIS_BUILD_SHARED OFF CACHE BOOL "" FORCE)
    set(VORBIS_BUILD_STATIC ON  CACHE BOOL "" FORCE)
    # 3. Force Vorbis and its sub-components to stay static
    set(BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)

    # 2. Fix the DEF file and Object handling for lld-link
    # This ensures that flags like /DEF: are passed correctly to the MSVC-style linker
    set(CMAKE_LINK_DEF_FILE_FLAG "/DEF:")

    # Explicitly link the UCRT libraries provided by xwin
    target_link_options(${PROJECT_NAME} PRIVATE "-Wl,/alternatename:m.lib=ucrt.lib")
    #set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -Wl,/alternatename:m.lib=ucrt.lib" CACHE STRING "" FORCE)
    #set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} -Wl,/alternatename:m.lib=ucrt.lib" CACHE STRING "" FORCE)
    target_link_libraries(${PROJECT_NAME}
            PRIVATE SDL3::SDL3 SDL3_mixer::SDL3_mixer zlibstatic src_lib)

    # target_compile_options(vorbisfile PRIVATE -mno-sse2)
    target_compile_definitions(vorbisfile PRIVATE _MM_NO_INTRINSICS)

    target_link_libraries(${PROJECT_NAME} PRIVATE ucrt.lib msvcrt.lib)
    add_definitions(-D_CRT_DECLARE_NONSTDC_NAMES=1)
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

