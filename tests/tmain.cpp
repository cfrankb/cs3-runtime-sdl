/*
    cs3-runtime-sdl
    Copyright (C) 2025 Francois Blanchette

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/
#include <cstdio>
#include <functional>
#include <vector>
#include <typeinfo>
#include <filesystem>
#include "t_game.h"
#include "t_gamestats.h"
#include "t_maparch.h"
#include "t_map.h"
#include "t_recorder.h"
#include "t_runtime.h"
#include "t_states.h"
#include "t_stateparser.h"
#include "t_strhelper.h"
#include "t_ifile.h"
#include "t_frameset.h"
#include "t_pngmagic.h"
#include "t_engine.h"
#include "t_actor.h"
#include "t_animator.h"
#include "t_randomz.h"
#include "t_parseargs.h"
#include "t_statedata.h"
#include "t_dotarray.h"
#include "t_bossdata.h"
#include "t_boss.h"
#include "t_level.h"
#include "t_ai_path.h"
#include "t_assetman.h"
#include "t_tables.h"
#include "t_game_ai.h"
#include "t_logger.h"
#include "t_gamemixin.h"
#include "t_menu.h"
#include "t_gameui.h"
#include "t_helper.h"
#include "t_frame.h"
#include "../src/logger.h"

#define FCT(x) {x, #x}
using Function = std::function<bool(void)>;
struct Test
{
    Function f;
    const char *s;
};

int main(int argc, char *args[])
{
    (void)argc;
    (void)args;

    std::filesystem::create_directories("tests/out");
    LOGI("running tests");
    LOGI("==============");

    std::vector<Test> fct = {
        FCT(test_recorder),
        FCT(test_states),
        FCT(test_stateparser),
        FCT(test_map),
        FCT(test_map_2),
        FCT(test_map_up),
        FCT(test_map_down),
        FCT(test_map_left),
        FCT(test_map_right),
        FCT(test_map_corrupt),
        FCT(test_maparch_1),
        FCT(test_maparch_2),
        FCT(test_maparch_3),
        FCT(test_maparch_corrupt),
        FCT(test_strhelper),
        FCT(test_gamestats),
        FCT(test_runtime),
        FCT(test_game),
        FCT(test_ifile),
        FCT(test_ifile_create),
        FCT(test_ifile_read_write),
        FCT(test_ifile_vector),
        FCT(test_ifile_string_bounds),
        FCT(test_ifile_negative_size),
        FCT(test_png_magic),
        FCT(test_png_refs),
        FCT(test_png_roundtrip),
        FCT(test_png_corrupt),
        FCT(test_png_orgpos),
        FCT(test_png_4bpp),
        FCT(test_png_filters),
        FCT(test_png_hostile_chunks),
        FCT(test_frameset),
        FCT(test_frameset_families),
        FCT(test_frameset_reject),
        FCT(test_frameset_container),
        FCT(test_frameset_hostile_imc1),
        FCT(test_actor),
        FCT(test_path_algos),
        FCT(test_animator),
        FCT(test_animator_sequences),
        FCT(test_animator_layer_tiles),
        FCT(test_animator_automators),
        FCT(test_animator_read_fail),
        FCT(test_engine_gather_sprites),
        FCT(test_engine_event_text),
        FCT(test_engine_level_summary),
        FCT(test_engine_resize_game_menu),
        FCT(test_engine_keyboard),
        FCT(test_engine_screens),
        FCT(test_random),
        FCT(test_parseargs),
        FCT(test_statedata),
        FCT(test_dotarray),
        FCT(test_bossdata),
        FCT(test_bossdata_hostile),
        FCT(test_boss),
        FCT(test_level),
        FCT(test_pathfinding),
        FCT(test_assetman),
        FCT(test_tables),
        FCT(test_logger),
        FCT(test_gamemixin),
        FCT(test_gamemixin_primitives),
        FCT(test_gamemixin_frames_routing),
        FCT(test_gamemixin_viewports),
        FCT(test_gamemixin_screen),
        FCT(test_gamemixin_screens),
        FCT(test_gamemixin_input),
        FCT(test_gamemixin_bosses),
        FCT(test_gamemixin_savegame),
        FCT(test_game_ai),
        FCT(test_menu_nav),
        FCT(test_menu_disabled),
        FCT(test_item_range),
        FCT(test_item_options),
        FCT(test_item_str),
        FCT(test_menu_roles),
        FCT(test_menu_scale),
        FCT(test_mgr_get),
        FCT(test_mgr_active),
        FCT(test_mgr_hit),
        FCT(test_gameui),
        FCT(test_helper_files),
        FCT(test_helper_compress),
        FCT(test_helper_strings),
        FCT(test_helper_uuid),
        FCT(test_frame_transforms),
        FCT(test_frame_fills),
        FCT(test_frame_geometry),
        FCT(test_frame_pixels),
        FCT(test_frame_io),
    };

    int failed = 0;
    for (size_t i = 0; i < fct.size(); ++i)
    {
        LOGI("==> %s()", fct[i].s);
        if (!fct[i].f())
        {
            LOGE("XXXXXX %s() FAILED", fct[i].s);
            ++failed;
        }
    }

    if (failed != 0)
    {
        LOGE("###### failed: %d", failed);
    }
    else
    {
        LOGI("###### completed");
    }
    return failed != 0;
}