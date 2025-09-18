cmake_minimum_required(VERSION 3.16)
project(MyGame LANGUAGES CXX)

set(CMAKE_VERBOSE_MAKEFILE ON)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_OSX_ARCHITECTURES "arm64;x86_64")


# Enable macOS bundle
set(MACOSX_BUNDLE_BUNDLE_NAME "CreepspreadIII")
set(MACOSX_BUNDLE_GUI_IDENTIFIER "com.cfrankb.creepspreadiii")
set(MACOSX_BUNDLE_SHORT_VERSION_STRING "1.0")
set(MACOSX_BUNDLE_LONG_VERSION_STRING "1.0.0")
set(MACOSX_BUNDLE_ICON_FILE "MyGame.icns")
set(MACOSX_BUNDLE_BUNDLE_VERSION "100")  # Optional: internal build number
set(MACOSX_BUNDLE TRUE)
# execute_process(COMMAND git describe --tags OUTPUT_VARIABLE GIT_TAG OUTPUT_STRIP_TRAILING_WHITESPACE)
# set(MACOSX_BUNDLE_SHORT_VERSION_STRING "${GIT_TAG}")


#find_package(SDL3 REQUIRED)
#find_package(SDL3_mixer REQUIRED)
add_subdirectory(external/SDL3)
add_subdirectory(external/SDL3_mixer)
find_library(XMP_LIBRARY xmp)
find_library(ZLIB_LIBRARY z)
find_library(OGG_LIBRARY ogg)
find_library(VORBIS_LIBRARY vorbisfile)

add_executable(MyGame MACOSX_BUNDLE
    src/animator.cpp
	src/recorder.cpp
	src/strhelper.cpp
	src/game.cpp
	src/actor.cpp
	src/statedata.cpp
	src/colormap.cpp
	src/menuitem.cpp
	src/states.cpp
	src/menu.cpp
	src/chars.cpp
	src/runtime.cpp
	src/map.cpp
	src/gamemixin.cpp
	src/level.cpp
	src/maparch.cpp
	src/main.cpp
	src/parseargs.cpp
	src/stateparser.cpp
	src/tilesdata.cpp
	src/gamestats.cpp
	src/shared/FileWrap.cpp
	src/shared/helper.cpp
	src/shared/DotArray.cpp
	src/shared/Frame.cpp
	src/shared/FrameSet.cpp
	src/shared/PngMagic.cpp
	src/shared/implementers/mu_sdl.cpp
	src/shared/implementers/sn_sdl.cpp
)

target_link_libraries(MyGame
    PRIVATE SDL3::SDL3 SDL3_mixer::SDL3_mixer
    "-framework CoreFoundation"
    PUBLIC ${XMP_LIBRARY}
    ${ZLIB_LIBRARY}
    ${OGG_LIBRARY}
    ${VORBIS_LIBRARY}
)
