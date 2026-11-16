cmake_minimum_required(VERSION 3.6)

project(GAME)

add_subdirectory(../../../external/SDL3 SDL3)
add_subdirectory(../../../external/SDL3_mixer SDL3_mixer)

add_library(main SHARED
        ../../../src/actor.cpp
        ../../../src/ai_path.cpp
        ../../../src/animator.cpp
        ../../../src/assetman.cpp
        ../../../src/boss.cpp
        ../../../src/bossdata.cpp
        ../../../src/colormap.cpp
        ../../../src/engine.cpp
        ../../../src/engine_hw.cpp
        ../../../src/game.cpp
        ../../../src/game_ai.cpp
        ../../../src/game_auto.cpp
        ../../../src/gamemixin.cpp
        ../../../src/gamestats.cpp
        ../../../src/gameui.cpp
        ../../../src/layer.cpp
        ../../../src/layerdata.cpp
        ../../../src/level.cpp
        ../../../src/logger.cpp
        ../../../src/main.cpp
        ../../../src/map.cpp
        ../../../src/maparch.cpp
        ../../../src/menu.cpp
        ../../../src/menuitem.cpp
        ../../../src/menumanager.cpp
        ../../../src/names.cpp
        ../../../src/parseargs.cpp
        ../../../src/randomz.cpp
        ../../../src/recorder.cpp
        ../../../src/runtime.cpp
        ../../../src/shared/DotArray.cpp
        ../../../src/shared/FileMem.cpp
        ../../../src/shared/FileWrap.cpp
        ../../../src/shared/Frame.cpp
        ../../../src/shared/FrameSet.cpp
        ../../../src/shared/PngMagic.cpp
        ../../../src/shared/helper.cpp
        ../../../src/shared/implementers/mu_sdl.cpp
        ../../../src/shared/implementers/sn_sdl.cpp
        ../../../src/sheetdata.cpp
        ../../../src/statedata.cpp
        ../../../src/states.cpp
        ../../../src/steam_helper.cpp
        ../../../src/strhelper.cpp
        ../../../src/tilesdata.cpp
        ../../../src/tileset_tex.cpp
)
target_link_libraries(main
    PRIVATE SDL3::SDL3 SDL3_mixer::SDL3_mixer
    z
    android
    log
)
