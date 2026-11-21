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

#include <vector>

#include "../src/boss.h"
#include "../src/game.h"
#include "../src/gamesfx.h"
#include "../src/gamestats.h"
#include "../src/logger.h"
#include "../src/map.h"
#include "../src/sprtypes.h"
#include "../src/tilesdata.h"
#include "../src/tilesdefs.h"
#include "../src/shared/FileMem.h"
#include "../src/shared/FileWrap.h"
#include "../src/tileset_tex.h"
#include <filesystem>
#include "t_fx.h"

// Extended fx tests for the Phase 1 contract (docs/spec/gamespec/
// extended-fx.md): spawn conversion, lifetime/onEnd, derived frame index,
// view culling and the save section. Synthetic g_fxData entries only — no
// data/ asset reads. Mutates the CGame singleton and g_fxData; both are
// restored by each test.

namespace
{
    constexpr int MAP_SIZE = 12;

    CGame *g_game = nullptr;
    int g_endCount = 0;

    void resetWorld()
    {
        g_game = CGame::getGame();
        CMap &map = CGame::getMap();
        map.clear();
        map.resize(MAP_SIZE, MAP_SIZE, TILES_BLANK, true);
        g_game->getMonsters().clear();
        g_game->getBosses().clear();
        g_game->getSfx().clear();
        g_game->getFx().clear();
        g_game->resetStats();
        g_game->clearEvents();
        g_game->player().move(MAP_SIZE - 1, MAP_SIZE - 1);
        g_game->rebuildMonsterGrid();
    }

    // append a synthetic table entry; truncated back on scope exit
    struct TableScope
    {
        const size_t m_saved = g_fxData.size();
        ~TableScope() { g_fxData.resize(m_saved); }
    };

    int addFx(const fxData_t &def)
    {
        g_fxData.push_back(def);
        return static_cast<int>(g_fxData.size()) - 1;
    }

    void tick(int n)
    {
        while (n--)
            g_game->tickFx();
    }

    bool checkEq(const long got, const long want, const char *what)
    {
        if (got != want)
        {
            LOGE("%s: got %ld, want %ld", what, got, want);
            return false;
        }
        return true;
    }
} // namespace

bool test_fx_spawn()
{
    resetWorld();
    TableScope table;
    const int id = addFx({.id = 0, .sheet = 0, .speed = 2, .flags = FX_ONESHOT,
                          .ttl = 0, .w = 32, .h = 48, .back = {0, 0}, .front = {0, 4},
                          .onEnd = nullptr});

    const int i0 = g_game->spawnFx(id, 5, 7);
    const int i1 = g_game->spawnFxPx(id, 100, 120);
    if (!checkEq(i0, 0, "spawnFx index") || !checkEq(i1, 1, "spawnFxPx index"))
        return false;
    if (g_game->getFx().size() != 2)
    {
        LOGE("spawn count %zu, want 2", g_game->getFx().size());
        return false;
    }

    const fx_t &tileFx = g_game->getFx()[0];
    if (!checkEq(tileFx.x, 5 * 16 + 8, "tile spawn x") ||
        !checkEq(tileFx.y, 7 * 16 + 8, "tile spawn y") ||
        !checkEq(tileFx.ox, 5, "tile spawn ox") ||
        !checkEq(tileFx.oy, 7, "tile spawn oy"))
        return false;

    const fx_t &pxFx = g_game->getFx()[1];
    if (!checkEq(pxFx.x, 100, "px spawn x") ||
        !checkEq(pxFx.y, 120, "px spawn y") ||
        !checkEq(pxFx.ox, -1, "px spawn ox") ||
        !checkEq(pxFx.oy, -1, "px spawn oy"))
        return false;

    if (!checkEq(g_game->spawnFx(9999, 1, 1), -1, "unknown effectID") ||
        !checkEq(g_game->spawnFxPx(-3, 0, 0), -1, "negative effectID"))
        return false;
    return g_game->getFx().size() == 2;
}

bool test_fx_lifetime()
{
    resetWorld();
    TableScope table;
    g_endCount = 0;

    // one-shot: 4 frames * speed 2 ends at timer 8
    const int oneShot = addFx({.id = 0, .sheet = 0, .speed = 2, .flags = FX_ONESHOT,
                               .ttl = 0, .w = 32, .h = 32, .back = {0, 0}, .front = {0, 4},
                               .onEnd = [](CGame &, const fx_t &) { ++g_endCount; }});
    if (g_game->spawnFxPx(oneShot, 50, 50) < 0)
        return false;
    tick(7);
    if (!checkEq(static_cast<long>(g_game->getFx().size()), 1, "one-shot at tick 7") ||
        !checkEq(g_endCount, 0, "one-shot onEnd early"))
        return false;
    tick(1);
    if (!checkEq(static_cast<long>(g_game->getFx().size()), 0, "one-shot at tick 8") ||
        !checkEq(g_endCount, 1, "one-shot onEnd count"))
        return false;

    // repeatable: survives past frames*speed, ends at ttl
    const int repeat = addFx({.id = 0, .sheet = 0, .speed = 2, .flags = FX_REPEATABLE,
                              .ttl = 12, .w = 32, .h = 32, .back = {0, 0}, .front = {0, 4},
                              .onEnd = [](CGame &, const fx_t &) { ++g_endCount; }});
    if (g_game->spawnFxPx(repeat, 50, 50) < 0)
        return false;
    tick(8); // frames*speed would have ended a one-shot
    if (!checkEq(static_cast<long>(g_game->getFx().size()), 1, "repeatable at tick 8"))
        return false;
    tick(4); // timer reaches ttl 12
    if (!checkEq(static_cast<long>(g_game->getFx().size()), 0, "repeatable at tick 12") ||
        !checkEq(g_endCount, 2, "repeatable onEnd count"))
        return false;

    // dangling effectID ends at once without a callback
    fx_t dangling{.x = 10, .y = 10, .ox = -1, .oy = -1, .effectID = 4044, .timer = 0};
    g_game->getFx().push_back(dangling);
    tick(1);
    return checkEq(static_cast<long>(g_game->getFx().size()), 0, "dangling erased") &&
           checkEq(g_endCount, 2, "dangling onEnd count");
}

bool test_fx_frame_index()
{
    TableScope table;
    const fxData_t oneShot{.id = 0, .sheet = 0, .speed = 2, .flags = FX_ONESHOT,
                           .ttl = 0, .w = 32, .h = 32, .back = {0, 0}, .front = {10, 4},
                           .onEnd = nullptr};
    const fxData_t repeat{.id = 0, .sheet = 0, .speed = 2, .flags = FX_REPEATABLE,
                          .ttl = 0, .w = 32, .h = 32, .back = {0, 0}, .front = {20, 4},
                          .onEnd = nullptr};
    fx_t fx{.x = 0, .y = 0, .ox = -1, .oy = -1, .effectID = 0, .timer = 0};

    struct Step { uint16_t timer; uint16_t oneShot; uint16_t repeat; };
    const Step steps[] = {
        {0, 10, 20}, {1, 10, 20}, {2, 11, 21}, {4, 12, 22},
        {6, 13, 23}, {8, 13, 20}, {10, 13, 21}, {100, 13, 22},
    };
    for (const Step &s : steps)
    {
        fx.timer = s.timer;
        if (!checkEq(fxFrameIndex(fx, oneShot, oneShot.front), s.oneShot, "one-shot frame") ||
            !checkEq(fxFrameIndex(fx, repeat, repeat.front), s.repeat, "repeatable frame"))
            return false;
    }
    // back layer base offset honored
    const fxData_t twoLayer{.id = 0, .sheet = 0, .speed = 1, .flags = FX_ONESHOT,
                            .ttl = 0, .w = 32, .h = 32, .back = {34, 6}, .front = {40, 6},
                            .onEnd = nullptr};
    fx.timer = 3;
    return checkEq(fxFrameIndex(fx, twoLayer, twoLayer.back), 37, "back base") &&
           checkEq(fxFrameIndex(fx, twoLayer, twoLayer.front), 43, "front base");
}

bool test_fx_cull()
{
    resetWorld();
    TableScope table;
    const int id = addFx({.id = 0, .sheet = 0, .speed = 1, .flags = FX_ONESHOT,
                          .ttl = 0, .w = 48, .h = 48, .back = {10, 2}, .front = {0, 2},
                          .onEnd = nullptr});
    const fxView_t view{0, 0, 200, 200};
    std::vector<fxDraw_t> out;

    // in view: both sides emit once, on their own base
    g_game->spawnFxPx(id, 100, 100);
    g_game->getFxToDraw(view, false, out);
    if (!checkEq(static_cast<long>(out.size()), 1, "back draw count") ||
        !checkEq(out[0].frameID, 10, "back frameID") ||
        !checkEq(out[0].x, 100, "back draw x"))
        return false;
    out.clear();
    g_game->getFxToDraw(view, true, out);
    if (!checkEq(static_cast<long>(out.size()), 1, "front draw count") ||
        !checkEq(out[0].frameID, 0, "front frameID"))
        return false;
    out.clear();

    // margin: center outside the raw rect, inside the w/2 = 24 inflation
    g_game->getFx().clear();
    g_game->spawnFxPx(id, 210, 100);
    g_game->getFxToDraw(view, true, out);
    if (!checkEq(static_cast<long>(out.size()), 1, "margin draw count"))
        return false;
    out.clear();

    // out of view: zero draws
    g_game->getFx().clear();
    g_game->spawnFxPx(id, 300, 100);
    g_game->getFxToDraw(view, true, out);
    g_game->getFxToDraw(view, false, out);
    return checkEq(static_cast<long>(out.size()), 0, "culled draw count");
}

bool test_fx_save_roundtrip()
{
    resetWorld();
    TableScope table;
    g_endCount = 0;
    const int id = addFx({.id = 0, .sheet = 0, .speed = 1, .flags = FX_ONESHOT,
                          .ttl = 0, .w = 32, .h = 32, .back = {0, 0}, .front = {0, 4},
                          .onEnd = [](CGame &, const fx_t &) { ++g_endCount; }});
    g_game->spawnFx(id, 3, 4);
    g_game->spawnFxPx(id, 150, 160);
    // a classic sfx alongside: its section must survive untouched
    g_game->getSfx().push_back(sfx_t{.x = 9, .y = 11, .sfxID = SFX_SPARKLE,
                                     .timeout = SFX_SPARKLE_TIMEOUT});

    CFileMem mem;
    if (!mem.open("tests/out/fx-save.dat", "rw+"))
    {
        LOGE("can't open tests/out/fx-save.dat");
        return false;
    }
    if (!g_game->write(mem) || !mem.seek(0))
    {
        LOGE("fx save write failed");
        mem.close();
        return false;
    }

    // mutate live state before restore
    g_game->getFx().clear();
    g_game->spawnFxPx(id, 1, 1);
    g_game->getSfx().clear();

    if (!g_game->read(mem))
    {
        LOGE("fx save read failed");
        mem.close();
        return false;
    }
    mem.close();

    if (!checkEq(static_cast<long>(g_game->getFx().size()), 2, "restored fx count"))
        return false;
    const fx_t &a = g_game->getFx()[0];
    const fx_t &b = g_game->getFx()[1];
    if (!checkEq(a.x, 3 * 16 + 8, "restored a.x") || !checkEq(a.y, 4 * 16 + 8, "restored a.y") ||
        !checkEq(a.ox, 3, "restored a.ox") || !checkEq(a.oy, 4, "restored a.oy") ||
        !checkEq(a.effectID, id, "restored a.effectID") || !checkEq(a.timer, 0, "restored a.timer") ||
        !checkEq(b.x, 150, "restored b.x") || !checkEq(b.y, 160, "restored b.y") ||
        !checkEq(b.ox, -1, "restored b.ox") || !checkEq(b.oy, -1, "restored b.oy"))
        return false;

    // classic sfx section unaffected
    if (!checkEq(static_cast<long>(g_game->getSfx().size()), 1, "restored sfx count"))
        return false;
    const sfx_t &sfx = g_game->getSfx()[0];
    if (!checkEq(sfx.x, 9, "restored sfx.x") || !checkEq(sfx.y, 11, "restored sfx.y") ||
        !checkEq(sfx.timeout, SFX_SPARKLE_TIMEOUT, "restored sfx.timeout"))
        return false;

    // restored instances still end through the table onEnd (4 frames * speed 1)
    tick(4);
    return checkEq(static_cast<long>(g_game->getFx().size()), 0, "restored fx erased") &&
           checkEq(g_endCount, 2, "restored fx onEnd count");
}

bool test_fx_sheet_meta()
{
    // synthetic obLT payload: u32 reserved, u32 version 2, u32 count,
    // then count * {u16 x, y, w, h} little-endian
    std::vector<uint8_t> blob;
    auto put32 = [&blob](uint32_t v)
    {
        for (int i = 0; i < 4; ++i)
            blob.push_back(static_cast<uint8_t>((v >> (8 * i)) & 0xff));
    };
    auto put16 = [&blob](uint16_t v)
    {
        for (int i = 0; i < 2; ++i)
            blob.push_back(static_cast<uint8_t>((v >> (8 * i)) & 0xff));
    };
    put32(0);
    put32(2);
    put32(2);
    put16(112); put16(0); put16(32); put16(48);
    put16(0); put16(144); put16(32); put16(48);

    std::vector<tileinfo_t> out;
    if (!parseFxSheetMeta(blob.data(), blob.size(), out))
    {
        LOGE("valid obLT payload rejected");
        return false;
    }
    if (!checkEq(static_cast<long>(out.size()), 2, "meta record count") ||
        !checkEq(static_cast<long>(out[0].x), 112, "record0 x") ||
        !checkEq(static_cast<long>(out[0].y), 0, "record0 y") ||
        !checkEq(static_cast<long>(out[0].w), 32, "record0 w") ||
        !checkEq(static_cast<long>(out[1].x), 0, "record1 x") ||
        !checkEq(static_cast<long>(out[1].y), 144, "record1 y") ||
        !checkEq(static_cast<long>(out[1].h), 48, "record1 h"))
        return false;

    // wrong version rejected
    blob[4] = 3;
    if (parseFxSheetMeta(blob.data(), blob.size(), out))
    {
        LOGE("bad version accepted");
        return false;
    }
    blob[4] = 2;

    // truncated records rejected
    blob.resize(blob.size() - 4);
    if (parseFxSheetMeta(blob.data(), blob.size(), out))
    {
        LOGE("truncated payload accepted");
        return false;
    }
    return true;
}

// Poison aura (docs/plans/poison-damage-plan.md D7): a derived draw entry
// emitted by getFxToDraw while S_POISON is non-zero -- centred on the
// player, one entry per layer, frames cycling the sheet range off a
// free-running m_fxTick, culled like any fx, and suppressed while the fx
// sheet is too short to hold the aura frames (assets land separately).
// g_fxSheet_data is forced to a synthetic size for determinism.
bool test_fx_poison_aura()
{
    resetWorld();
    const fxData_t *def = fxDataAt(FX_POISON_AURA);
    if (!def)
    {
        LOGE("poison aura: table entry missing");
        return false;
    }
    const size_t saved = g_fxSheet_data.size();
    g_fxSheet_data.resize(static_cast<size_t>(def->back.base) + def->back.count);
    bool ok = true;

    // pool set directly; clock kept high so decTimers ticks never drain
    g_game->stats().set(S_POISON, 10);
    g_game->stats().set(S_POISON_NEXT, 1000);
    const fxView_t view{0, 0, 320, 320};
    const int px = (MAP_SIZE - 1) * 16 + def->w / 2;
    const int py = (MAP_SIZE - 1) * 16 + def->h / 2;

    std::vector<fxDraw_t> front;
    std::vector<fxDraw_t> back;
    g_game->getFxToDraw(view, true, front);
    g_game->getFxToDraw(view, false, back);
    if (front.size() != 1 || back.size() != 1)
    {
        LOGE("poison aura: front %zu back %zu entries", front.size(), back.size());
        ok = false;
    }
    else if (front[0].x != px || front[0].y != py || back[0].x != px || back[0].y != py)
    {
        LOGE("poison aura: anchor %d,%d want %d,%d", front[0].x, front[0].y, px, py);
        ok = false;
    }
    else if (front[0].frameID < def->front.base ||
             front[0].frameID >= def->front.base + def->front.count ||
             back[0].frameID < def->back.base ||
             back[0].frameID >= def->back.base + def->back.count)
    {
        LOGE("poison aura: frame %u/%u outside sheet range",
             front[0].frameID, back[0].frameID);
        ok = false;
    }
    else
    {
        // clock advance: one speed-period of decTimers shifts the cycle by 1
        const uint16_t first = front[0].frameID;
        for (int i = 0; i < def->speed; ++i)
            g_game->decTimers();
        front.clear();
        g_game->getFxToDraw(view, true, front);
        const uint16_t want =
            def->front.base +
            static_cast<uint16_t>(first - def->front.base + 1) % def->front.count;
        ok = front.size() == 1 && front[0].frameID == want &&
             g_game->stats().get(S_POISON) == 10; // clock never drained the pool
        if (!ok)
            LOGE("poison aura: cycle %u want %u", front.empty() ? 0xFFFF : front[0].frameID, want);
    }

    // culled when off-view
    const fxView_t tiny{0, 0, 8, 8};
    front.clear();
    g_game->getFxToDraw(tiny, true, front);
    ok = ok && front.empty();

    // pool cleared -> no entry
    g_game->stats().set(S_POISON, 0);
    front.clear();
    g_game->getFxToDraw(view, true, front);
    ok = ok && front.empty();

    // short sheet -> suppressed even while poisoned
    g_fxSheet_data.resize(0);
    g_game->stats().set(S_POISON, 10);
    front.clear();
    g_game->getFxToDraw(view, true, front);
    ok = ok && front.empty();

    g_fxSheet_data.resize(saved);
    g_game->resetStats();
    return ok;
}

bool test_fx_sheet_tail_scan()
{
    // the loader must find an obLT chunk appended at the END of the file,
    // not one assumed right after the PNG header. The chunk's crc field is
    // the literal "obLT" as a decoy for the backward tag scan.
    const char *path = "tests/out/fx_tail_scan.png";
    std::filesystem::create_directories("tests/out");

    const uint8_t payload[] = {
        0, 0, 0, 0, 2, 0, 0, 0, 2, 0, 0, 0,
        112, 0, 0, 0, 32, 0, 48, 0,
        0, 0, 144, 0, 32, 0, 48, 0};
    const uint32_t len = sizeof(payload);
    std::vector<uint8_t> blob = {137, 80, 78, 71, 13, 10, 26, 10};
    blob.resize(64, 0); // junk ahead of the chunk
    blob.push_back(static_cast<uint8_t>(len >> 24));
    blob.push_back(static_cast<uint8_t>(len >> 16));
    blob.push_back(static_cast<uint8_t>(len >> 8));
    blob.push_back(static_cast<uint8_t>(len));
    blob.insert(blob.end(), {'o', 'b', 'L', 'T'});
    blob.insert(blob.end(), payload, payload + len);
    blob.insert(blob.end(), {'o', 'b', 'L', 'T'}); // decoy crc

    CFileWrap file;
    bool ok = file.open(path, "wb") &&
              file.write(blob.data(), static_cast<int>(blob.size())) == IFILE_OK;
    file.close();
    if (!ok)
    {
        LOGE("can't write %s", path);
        return false;
    }

    // no renderer: loadFx must still locate and parse the tail chunk (it
    // fills tiledata) before failing later on the texture load.
    std::vector<tileinfo_t> out;
    TileSet sheet;
    if (sheet.loadFx(nullptr, path, out))
    {
        LOGE("loadFx succeeded without a renderer");
        std::filesystem::remove(path);
        return false;
    }
    std::filesystem::remove(path);
    if (!checkEq(static_cast<long>(out.size()), 2, "tail scan record count") ||
        !checkEq(static_cast<long>(out[0].x), 112, "tail record0 x") ||
        !checkEq(static_cast<long>(out[1].y), 144, "tail record1 y"))
        return false;

    // a file with no obLT chunk must leave tiledata empty
    blob.resize(16, 0);
    if (!file.open(path, "wb"))
    {
        LOGE("can't write %s", path);
        return false;
    }
    file.write(blob.data(), static_cast<int>(blob.size()));
    file.close();
    if (sheet.loadFx(nullptr, path, out) || !out.empty())
    {
        LOGE("loadFx found obLT in a file without one");
        std::filesystem::remove(path);
        return false;
    }
    std::filesystem::remove(path);
    return true;
}
