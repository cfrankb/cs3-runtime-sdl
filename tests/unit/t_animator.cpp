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

#include <cstring>
#include <filesystem>
#include "../../src/shared/FileMem.h"
#include "../../src/shared/FileWrap.h"
#include "../../src/logger.h"
#include "../../src/animator.h"
#include "../../src/game.h"
#include "../../src/map.h"
#include "../../src/layer.h"
#include "../../src/layerdata.h"
#include "../../src/tilesdata.h"
#include "../../src/animzdata.h"
#include "t_animator.h"

bool test_animator()
{
    constexpr const char *OUT_FILE = "tests/out/animator.dat";

    auto &automators = CGame::getGame()->automators();
    automators.clear();
    automators.push_back({0x1234, 10, 20, 1, 30, 0x40, 1});
    automators.push_back({0x0001, -5, 3, 0, 0, 0x11, 1});

    CAnimator animator1;
    CFileMem mem;
    if (!mem.open(OUT_FILE, "rw+"))
    {
        LOGE("can't write %s", OUT_FILE);
        return false;
    }
    if (!animator1.write(mem))
    {
        LOGE("failed to serialize CAnimator");
        mem.close();
        return false;
    }

    // simulate restore-from-save: wipe game state, then read it back
    automators.clear();
    if (!mem.seek(0))
    {
        LOGE("seek(0) failed");
        mem.close();
        return false;
    }
    CAnimator animator2;
    if (!animator2.read(mem))
    {
        LOGE("failed to deserialize CAnimator");
        mem.close();
        return false;
    }
    mem.close();

    if (automators.size() != 2)
    {
        LOGE("automators restored %zu; expecting 2", automators.size());
        return false;
    }
    const uint16_t want[] = {0x1234, 0x0001};
    for (size_t i = 0; i < 2; ++i)
    {
        if (automators[i].tileID != want[i] ||
            automators[i].x != (int16_t)(i ? -5 : 10) ||
            automators[i].y != (int16_t)(i ? 3 : 20))
        {
            LOGE("automator[%zu] mismatch", i);
            return false;
        }
    }

    // tile animation tables must survive the round-trip
    for (uint8_t tileID = 0; tileID < 0xff; tileID += 0x11)
    {
        if (animator2.at(tileID) != animator1.at(tileID))
        {
            LOGE("animator tile 0x%.2x mismatch", tileID);
            return false;
        }
    }

    automators.clear();
    std::filesystem::remove(OUT_FILE);
    return true;
}

namespace
{
// scratch g_layerdata slots, unused by the animator by default (t_game pattern)
constexpr uint16_t ID_A = 0x43;
constexpr uint16_t ID_B = 0x44;
constexpr uint16_t ID_C = 0x45;

struct Scratch
{
    uint16_t idx;
    layerdata_t saved;
};

void patchScratch(Scratch *s, size_t n)
{
    for (size_t i = 0; i < n; ++i)
    {
        s[i] = {static_cast<uint16_t>(ID_A + i), g_layerdata[ID_A + i]};
        memset(&g_layerdata[ID_A + i], 0, sizeof(layerdata_t));
    }
}

void restoreScratch(const Scratch *s, size_t n)
{
    for (size_t i = 0; i < n; ++i)
    {
        g_layerdata[s[i].idx] = s[i].saved;
    }
}
} // namespace

bool test_animator_sequences()
{
    CAnimator animator;

    // main-layer sequence: start..start+len-1 then wrap
    constexpr uint16_t START = ANIMZ_DIAMOND;
    constexpr uint16_t LEN = ANIMZ_DIAMOND_LEN;
    for (uint16_t k = 0; k < LEN + 2; ++k)
    {
        animator.animate();
        if (animator.at(TILES_DIAMOND) != START + (k % LEN))
        {
            LOGE("diamond after %u ticks = %u; expecting %u", k,
                 animator.at(TILES_DIAMOND), START + (k % LEN));
            return false;
        }
        if (animator.offset() != k + 1)
        {
            LOGE("offset %u; expecting %u", animator.offset(), k + 1);
            return false;
        }
    }

    // special-case lookup
    if (!animator.isSpecialCase(TILES_ZOMBIE) ||
        !animator.isSpecialCase(TILES_EGG_WHOLE) || animator.isSpecialCase(TILES_BLANK))
    {
        LOGE("isSpecialCase membership wrong");
        return false;
    }
    const animzInfo_t diam = animator.getSpecialInfo(TILES_DIAMOND);
    if (diam.frames != LEN || diam.base != 0 ||
        diam.offset != animator.offset() % LEN)
    {
        LOGE("diamond special info frames %u base %u offset %u", diam.frames,
             diam.base, diam.offset);
        return false;
    }
    const animzInfo_t insect = animator.getSpecialInfo(TILES_INSECT1);
    if (insect.frames != ANIMZ_INSECT1_LEN || insect.base != ANIMZ_INSECT1)
    {
        LOGE("insect special info frames %u base %u", insect.frames, insect.base);
        return false;
    }
    const animzInfo_t unknown = animator.getSpecialInfo(TILES_BLANK);
    if (unknown.frames != 0 || unknown.base != 0 || unknown.offset != 0)
    {
        LOGE("unknown tile returned special info");
        return false;
    }
    return true;
}

bool test_animator_layer_tiles()
{
    Scratch scratch[3];
    patchScratch(scratch, 3);
    g_layerdata[ID_A].nextTile = ID_B;
    g_layerdata[ID_A].animeSpeed = 2;
    g_layerdata[ID_C].nextTile = ID_A;
    g_layerdata[ID_C].manual = true;

    CAnimator animator;
    animator.reloadTileData();
    if (animator.getLayerTile(ID_A) != ID_A)
    {
        LOGE("animated tile A not registered (%u)", animator.getLayerTile(ID_A));
        restoreScratch(scratch, 3);
        return false;
    }
    if (animator.getLayerTile(ID_B) != ID_B) // entry stays 0 -> identity
    {
        LOGE("idle tile B maps to %u", animator.getLayerTile(ID_B));
        restoreScratch(scratch, 3);
        return false;
    }
    if (animator.getLayerTile(ID_C) != ID_C) // manual -> not registered
    {
        LOGE("manual tile C was registered");
        restoreScratch(scratch, 3);
        return false;
    }

    animator.animate(); // offset 1: 1 % 2 != 0
    if (animator.getLayerTile(ID_A) != ID_A)
    {
        LOGE("tile A advanced too early");
        restoreScratch(scratch, 3);
        return false;
    }
    animator.animate(); // offset 2: advance to nextTile
    if (animator.getLayerTile(ID_A) != ID_B)
    {
        LOGE("tile A did not advance (got %u)", animator.getLayerTile(ID_A));
        restoreScratch(scratch, 3);
        return false;
    }
    restoreScratch(scratch, 3);

    // out-of-range layer tile: logged and skipped, must not crash
    constexpr const char *OUT_FILE = "tests/out/animator_oob.dat";
    {
        CFileWrap file;
        if (!file.open(OUT_FILE, "wb"))
        {
            LOGE("cannot create %s", OUT_FILE);
            return false;
        }
        CAnimator src;
        src.write(file); // well-formed stream with empty automators
        file.close();
        // poke tileLayer slot 3 (after 512-byte main layer) past TOTAL_TILE_COUNT
        if (!file.open(OUT_FILE, "rb+"))
        {
            LOGE("cannot reopen %s for patching", OUT_FILE);
            return false;
        }
        const uint16_t oob = 0x1000;
        const bool poked = file.seek(512 + 3 * sizeof(uint16_t)) &&
                           file.write(&oob, sizeof(oob)) == IFILE_OK;
        file.close();
        if (!poked)
        {
            LOGE("failed to poke out-of-range tile");
            return false;
        }

        CFileWrap file2;
        if (!file2.open(OUT_FILE, "rb"))
        {
            LOGE("cannot reopen %s", OUT_FILE);
            return false;
        }
        CAnimator oobAnim;
        if (!oobAnim.read(file2))
        {
            LOGE("out-of-range stream rejected by read");
            file2.close();
            std::filesystem::remove(OUT_FILE);
            return false;
        }
        file2.close();
        oobAnim.animate(); // exercises the LOGW guard
        CGame::getGame()->automators().clear();
    }
    std::filesystem::remove(OUT_FILE);
    return true;
}

bool test_animator_automators()
{
    Scratch scratch[2];
    patchScratch(scratch, 2);
    g_layerdata[ID_A].nextTile = ID_B;
    g_layerdata[ID_A].animeSpeed = 3;

    CGame *game = CGame::getGame();
    CMap &map = game->getMap();
    map.clear();
    map.resize(8, 8, TILES_BLANK, true);
    CLayer *layer = map.addLayer(CLayer::LAYER_DECOR, "animprobe", 1);
    if (!layer)
    {
        LOGE("addLayer failed");
        restoreScratch(scratch, 2);
        return false;
    }
    const uint16_t layerID = static_cast<uint16_t>(map.layerCount() - 1);
    layer->set(2, 2, ID_A);
    layer->set(5, 5, TILES_BLANK);

    auto &automators = game->automators();
    automators.clear();
    automators.push_back({ID_A, 2, 2, layerID, 2, 0, 1});  // ttl burns then advances
    automators.push_back({0, 5, 5, layerID, 0, 0, 1});     // blank tile -> deactivates
    automators.push_back({0, 1, 1, layerID, 0, 0, 0});     // inactive -> untouched

    CAnimator animator;
    animator.animate(); // automator 0: ttl 2 -> 1
    if (automators[0].ttl != 1 || layer->at(2, 2) != ID_A)
    {
        LOGE("tick 1: ttl %u tile 0x%x", automators[0].ttl, layer->at(2, 2));
        goto fail;
    }
    if (automators[1].active != 0)
    {
        LOGE("blank-tile automator still active after tick 1");
        goto fail;
    }
    animator.animate(); // ttl 1 -> 0
    if (automators[0].ttl != 0)
    {
        LOGE("tick 2: ttl %u", automators[0].ttl);
        goto fail;
    }
    animator.animate(); // advance A -> B on the layer
    if (layer->at(2, 2) != ID_B || automators[0].tileID != ID_B ||
        automators[0].ttl != g_layerdata[ID_B].animeSpeed)
    {
        LOGE("tick 3: tile 0x%x automator tileID 0x%x ttl %u", layer->at(2, 2),
             automators[0].tileID, automators[0].ttl);
        goto fail;
    }
    animator.animate(); // B has no nextTile -> deactivate
    if (automators[0].active != 0)
    {
        LOGE("terminal automator still active");
        goto fail;
    }
    if (automators[2].active != 0)
    {
        LOGE("inactive automator was touched");
        goto fail;
    }

    automators.clear();
    map.clear();
    restoreScratch(scratch, 2);
    return true;
fail:
    automators.clear();
    map.clear();
    restoreScratch(scratch, 2);
    return false;
}

bool test_animator_read_fail()
{
    CFileMem empty;
    if (!empty.open("tests/out/animator_empty.dat", "rw+"))
    {
        LOGE("cannot create empty scratch");
        return false;
    }
    CAnimator a;
    if (a.read(empty))
    {
        LOGE("read accepted an empty stream");
        empty.close();
        return false;
    }
    empty.close();
    std::filesystem::remove("tests/out/animator_empty.dat");

    // main layer present, tile layer truncated
    CFileMem partial;
    if (!partial.open("tests/out/animator_part.dat", "rw+"))
    {
        return false;
    }
    std::vector<uint8_t> zeros(512, 0);
    partial.write(zeros.data(), static_cast<int>(zeros.size()));
    partial.seek(0);
    CAnimator b;
    if (b.read(partial))
    {
        LOGE("read accepted a truncated stream");
        partial.close();
        return false;
    }
    partial.close();
    std::filesystem::remove("tests/out/animator_part.dat");
    return true;
}

// maintainer ruling 2026-11-18 (animator ingest): tileLayer entries at or
// past TOTAL_TILE_COUNT are cleared to 0 so getLayerTile can never hand a
// refID outside g_layerdata to getLayerTileDef
bool test_animator_read_clamp()
{
    CFileMem file;
    if (!file.open("tests/out/animator_clamp.dat", "rw+"))
    {
        LOGE("cannot create clamp scratch");
        return false;
    }
    std::vector<uint16_t> main(256, 0xffff); // NO_ANIMZ filler (CAnimator::MAX_TILES)
    file.write(main.data(), static_cast<int>(main.size() * sizeof(uint16_t)));
    std::vector<uint16_t> table(TOTAL_TILE_COUNT, 0);
    table[0] = TOTAL_TILE_COUNT;     // first value outside the table
    table[1] = 0xffff;               // hostile sentinel
    table[2] = TOTAL_TILE_COUNT - 1; // last valid entry must survive
    file.write(table.data(), static_cast<int>(table.size() * sizeof(uint16_t)));
    const uint32_t noAutomators = 0;
    file.write(&noAutomators, sizeof(noAutomators));
    file.seek(0);

    CAnimator anim;
    if (!anim.read(file))
    {
        LOGE("read rejected the forged-but-well-formed stream");
        file.close();
        return false;
    }
    file.close();
    std::filesystem::remove("tests/out/animator_clamp.dat");

    // cleared entries behave like never-animated entries (getLayerTile
    // falls back to the queried tileID)
    if (anim.getLayerTile(0) != 0 || anim.getLayerTile(1) != 1)
    {
        LOGE("read_clamp: hostile entries survived");
        return false;
    }
    if (anim.getLayerTile(2) != TOTAL_TILE_COUNT - 1)
    {
        LOGE("read_clamp: valid entry was cleared");
        return false;
    }
    return true;
}
