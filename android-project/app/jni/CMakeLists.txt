cmake_minimum_required(VERSION 3.6)

project(GAME)

add_subdirectory(../../../external/SDL3 SDL3)
add_subdirectory(../../../external/SDL3_mixer SDL3_mixer)

add_library(main SHARED
        ../../../src/assetman.cpp
        ../../../src/animator.cpp
        ../../../src/recorder.cpp
        ../../../src/strhelper.cpp
        ../../../src/game.cpp
        ../../../src/gameui.cpp
        ../../../src/actor.cpp
        ../../../src/statedata.cpp
        ../../../src/colormap.cpp
        ../../../src/menuitem.cpp
        ../../../src/states.cpp
        ../../../src/menu.cpp
        ../../../src/chars.cpp
        ../../../src/runtime.cpp
        ../../../src/map.cpp
        ../../../src/gamemixin.cpp
        ../../../src/level.cpp
        ../../../src/maparch.cpp
        ../../../src/main.cpp
        ../../../src/parseargs.cpp
        ../../../src/tilesdata.cpp
        ../../../src/gamestats.cpp
        ../../../src/shared/FileWrap.cpp
        ../../../src/shared/FileMem.cpp
        ../../../src/shared/helper.cpp
        ../../../src/shared/DotArray.cpp
        ../../../src/shared/Frame.cpp
        ../../../src/shared/FrameSet.cpp
        ../../../src/shared/PngMagic.cpp
        ../../../src/shared/implementers/mu_sdl.cpp
        ../../../src/shared/implementers/sn_sdl.cpp
)
target_link_libraries(main
    PRIVATE SDL3::SDL3 SDL3_mixer::SDL3_mixer
    z
    android
    log
)


# SDL sources are in a subfolder named "SDL"
#add_subdirectory(external/SDL3)
#add_subdirectory(external/SDL3_mixer)
#add_subdirectory(external/libxmp)


# Compilation of companion libraries
#add_subdirectory(SDL_image)
#add_subdirectory(SDL_mixer)
#add_subdirectory(SDL_ttf)

# Your game and its CMakeLists.txt are in a subfolder named "src"
#add_subdirectory(src)

