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
#include <string>
#include <unordered_map>
#include "t_gamemixin.h"
#include "../src/runtime.h"
#include "../src/game.h"
#include "../src/gamestats.h"
#include "../src/states.h"
#include "../src/statedata.h"
#include "../src/events.h"
#include "../src/actor.h"
#include "../src/logger.h"
#include "../src/shared/Frame.h"
#include "../src/shared/FrameSet.h"
#include "../src/shared/FileMem.h"
#include "../src/map.h"
#include "../src/animator.h"
#include "../src/tilesdata.h"
#include "../src/tilesdefs.h"
#include "../src/animzdata.h"
#include "../src/attr.h"
#include "../src/chars.h"
#include "../src/colormap.h"
#include "../src/colormasks.h"
#include "../src/boss.h"
#include "../src/bossdata.h"

namespace fs = std::filesystem;

namespace
{
// CRuntime is default-constructible headless (see t_runtime); the mixin
// members under test are protected, so expose them through a subclass.
class TestRuntime : public CRuntime
{
public:
    using CRuntime::clearScores;
    using CRuntime::getEventText;
    using CRuntime::manageTimer;
    using CRuntime::moveCamera;
    using CRuntime::rankUserScore;
    using CRuntime::INVALID;
    using CRuntime::MAX_SCORES;

    // Phase E: draw / input / save entry points
    using CRuntime::calcSpecialFrame;
    using CRuntime::cameraSpeed;
    using CRuntime::centerCamera;
    using CRuntime::clearKeyStates;
    using CRuntime::drawBossses;
    using CRuntime::drawEventText;
    using CRuntime::drawFont;
    using CRuntime::drawFont6x6;
    using CRuntime::drawGameStatus;
    using CRuntime::drawHealthBar;
    using CRuntime::drawHelpScreen;
    using CRuntime::drawKeys;
    using CRuntime::drawLevelIntro;
    using CRuntime::drawPreScreen;
    using CRuntime::drawRect;
    using CRuntime::drawScroll;
    using CRuntime::drawScores;
    using CRuntime::drawScreen;
    using CRuntime::drawTile;
    using CRuntime::drawTileFaz;
    using CRuntime::drawTimeout;
    using CRuntime::drawUI;
    using CRuntime::drawViewPortDynamic;
    using CRuntime::drawViewPortStatic;
    using CRuntime::enableHiScore;
    using CRuntime::fazeScreen;
    using CRuntime::flashScreen;
    using CRuntime::gatherSprites;
    using CRuntime::handleFunctionKeys;
    using CRuntime::handleFunctionKeys_Game;
    using CRuntime::handleFunctionKeys_General;
    using CRuntime::handleInputString;
    using CRuntime::handlePrompts;
    using CRuntime::plotLine;
    using CRuntime::read;
    using CRuntime::setCameraMode;
    using CRuntime::setSkill;
    using CRuntime::startCountdown;
    using CRuntime::tile2Frame;
    using CRuntime::whichButton;
    using CRuntime::write;

    // constants used by the fixture and assertions
    using CRuntime::CAMERA_MODE_DYNAMIC;
    using CRuntime::CAMERA_MODE_STATIC;
    using CRuntime::CARET_SPEED;
    using CGameMixin::FONT_SIZE; // CRuntime shadows FONT_SIZE privately
    using CRuntime::HEALTHBAR_CLASSIC;
    using CRuntime::HEALTHBAR_HEARTHS;
    using CRuntime::INDEX_PLAYER_DEAD;
    using CRuntime::INTRO_DELAY;
    using CRuntime::KEY_NO_REPETE;
    using CRuntime::KEY_REPETE_DELAY;
    using CRuntime::PLAYER_BOAT_FRAME;
    using CRuntime::PLAYER_DOWN_INDEX;
    using CRuntime::PLAYER_IDLE_BASE;
    using CRuntime::PLAYER_TOTAL_FRAMES;
    using CRuntime::TICK_RATE;
    using CRuntime::TILE_SIZE;
    using CRuntime::Y_STATUS;
    using CRuntime::Key_0;
    using CRuntime::Key_9;
    using CRuntime::Key_A;
    using CRuntime::Key_BackSpace;
    using CRuntime::Key_Enter;
    using CRuntime::Key_Escape;
    using CRuntime::Key_F1;
    using CRuntime::Key_F2;
    using CRuntime::Key_F3;
    using CRuntime::Key_F4;
    using CRuntime::Key_F9;
    using CRuntime::Key_F10;
    using CRuntime::Key_F11;
    using CRuntime::Key_F12;
    using CRuntime::Key_N;
    using CRuntime::Key_Period;
    using CRuntime::Key_Space;
    using CRuntime::Key_Y;
    using CRuntime::PROMPT_ERASE_SCORES;
    using CRuntime::PROMPT_HARDCORE;
    using CRuntime::PROMPT_LOAD;
    using CRuntime::PROMPT_NONE;
    using CRuntime::PROMPT_RESTART_GAME;
    using CRuntime::PROMPT_SAVE;
    using CRuntime::PROMPT_TOGGLE_MUSIC;
    using CRuntime::sprite_t;
    using CRuntime::cameraContext_t;
    using CRuntime::visualCues_t;

    int &event() { return m_currentEvent; }
    int &timer() { return m_timer; }
    uint32_t &ticks() { return m_ticks; }
    int &camX() { return m_cx; }
    int &camY() { return m_cy; }
    hiscore_t &scoreAt(int i) { return m_hiscores[i]; }
    int width_() { return getWidth(); }
    int height_() { return getHeight(); }

    CAnimator *animator_() { return m_animator; }
    std::vector<uint8_t> &fontData() { return m_fontData; }
    std::unique_ptr<CFrameSet> &tiles_() { return m_tiles; }
    std::unique_ptr<CFrameSet> &animz_() { return m_animz; }
    std::unique_ptr<CFrameSet> &users_() { return m_users; }
    std::unique_ptr<CFrameSet> &sheet0_() { return m_sheet0; }
    std::unique_ptr<CFrameSet> &sheet1_() { return m_sheet1; }
    std::unique_ptr<CFrameSet> &uisheet_() { return m_uisheet; }
    std::unique_ptr<CFrameSet> &layerTiles_() { return m_layerTiles; }
    CGameUI &ui_() { return m_ui; }
    Prompt &prompt_() { return m_prompt; }
    bool &paused_() { return m_paused; }
    int &musicMuted_() { return m_musicMuted; }
    int &playerFrameOffset_() { return m_playerFrameOffset; }
    int &healthRef_() { return m_healthRef; }
    int &countdown_() { return m_countdown; }
    int &healthBar_() { return m_healthBar; }
    int &cameraMode_() { return m_cameraMode; }
    bool &recordScore_() { return m_recordScore; }
    bool &hiscoreEnabled_() { return m_hiscoreEnabled; }
    int &scoreRank_() { return m_scoreRank; }
    uint8_t &keyState_(int k) { return m_keyStates[k]; }
    uint8_t &keyRepeter_(int k) { return m_keyRepeters[k]; }
    visualStates_t &visual_() { return m_visualStates; }
    ColorMaps &colormaps_() { return m_colormaps; }
    void sugarMeter_(CFrame &bitmap, int bx) { drawSugarMeter(bitmap, bx); }
};

bool checkMessage(TestRuntime &rt, int eventId, const std::string &line,
                  Color color, int baseY, const char *what)
{
    rt.event() = eventId;
    const auto msg = rt.getEventText(baseY);
    if (msg.lines[0] != line || msg.color != color || msg.baseY != baseY)
    {
        LOGE("test_gamemixin: %s: got '%s' color %d baseY %d", what,
             msg.lines[0].c_str(), (int)msg.color, msg.baseY);
        return false;
    }
    return true;
}

// --- Phase E fixture -------------------------------------------------------
// Solid-colour frames make every draw path deterministic: one known pixel
// colour per frame index. The font data is filled with 0xff so every glyph
// (including custom chars) renders as a solid FONT_SIZE block.
constexpr uint32_t solidColor(int i, int seed)
{
    const uint32_t r = 1 + (i * 7 + seed * 11) % 255;
    const uint32_t g = 1 + (i * 13 + seed * 17) % 255;
    const uint32_t b = 1 + (i * 29 + seed * 23) % 255;
    return 0xff000000u | (b << 16) | (g << 8) | r;
}

constexpr uint32_t fazFilter_(int shift)
{
    return (0xff >> shift) << 16 | (0xff >> shift) << 8 | 0xff >> shift;
}

constexpr uint32_t fadedColor(uint32_t c, int shift)
{
    return ((c >> shift) & fazFilter_(shift)) | 0xff000000u;
}

constexpr uint32_t grayColor(uint32_t c)
{
    uint32_t avg = ((c >> 16) & 0xff) + ((c >> 8) & 0xff) + (c & 0xff);
    avg /= 3;
    return (c & 0xff000000u) | avg * 0x010101u;
}

std::unique_ptr<CFrameSet> solidSet(int count, int w, int h, int seed)
{
    auto set = std::make_unique<CFrameSet>();
    for (int i = 0; i < count; ++i)
    {
        auto *frame = new CFrame(w, h);
        std::vector<uint32_t> rgb(w * h, solidColor(i, seed));
        frame->setRGB(rgb);
        set->add(frame);
    }
    return set;
}

void installFixture(TestRuntime &rt)
{
    // (0xFF - CHARS_CUSTOM + CHARS_CUSTOM_BASE + 1) glyph slots * FONT_SIZE
    rt.fontData().assign((0xff - 0x80 + 0x5f + 1) * TestRuntime::FONT_SIZE, 0xff);
    rt.tiles_() = solidSet(256, 16, 16, 1);
    rt.animz_() = solidSet(512, 16, 16, 2);
    rt.users_() = solidSet(TestRuntime::PLAYER_TOTAL_FRAMES * 4, 16, 16, 3);
    rt.uisheet_() = solidSet(3, 16, 48, 4);
    rt.layerTiles_() = solidSet(256, 16, 16, 7);
    rt.sheet0_() = solidSet(1024, 16, 16, 5);
    rt.sheet1_() = solidSet(1024, 16, 16, 6);
}

uint32_t px(const CFrame &f, int x, int y)
{
    return f.getPixelsConst()[(size_t)y * f.width() + x];
}

int nonBlack(const CFrame &f, int x0, int y0, int x1, int y1)
{
    int count = 0;
    for (int y = y0; y < y1; ++y)
        for (int x = x0; x < x1; ++x)
            if (px(f, x, y) != BLACK)
                ++count;
    return count;
}

bool fail(const char *what)
{
    LOGE("test_gamemixin: FAILED %s", what);
    return false;
}
}

// --- Phase E: drawing primitives -------------------------------------------
bool test_gamemixin_primitives()
{
    TestRuntime rt;
    installFixture(rt);
    CFrame bmp(64, 64);

    // 0xff glyph bytes => solid 8x8 block per char
    bmp.fill(BLACK);
    rt.drawFont(bmp, 0, 0, "A", WHITE);
    if (px(bmp, 0, 0) != WHITE || px(bmp, 7, 7) != WHITE ||
        px(bmp, 8, 0) != BLACK || px(bmp, 0, 8) != BLACK)
        return fail("drawFont solid block");

    bmp.fill(BLACK);
    rt.drawFont(bmp, 4, 4, "A", YELLOW, BLACK, 2, 3);
    if (px(bmp, 4, 4) != YELLOW || px(bmp, 19, 27) != YELLOW ||
        px(bmp, 20, 4) != BLACK || px(bmp, 4, 28) != BLACK)
        return fail("drawFont scale 2x3");

    // custom glyph (>= CHARS_CUSTOM) reads at +CHARS_CUSTOM_BASE offset
    bmp.fill(BLACK);
    rt.drawFont(bmp, 0, 0, "\x80\xFF", RED);
    if (px(bmp, 0, 0) != RED || px(bmp, 8, 0) != RED)
        return fail("drawFont custom chars");

    // drawFont6x6: 6x6 block per char, no scaling
    bmp.fill(BLACK);
    rt.drawFont6x6(bmp, 1, 1, "AB", CYAN);
    if (px(bmp, 1, 1) != CYAN || px(bmp, 6, 6) != CYAN ||
        px(bmp, 7, 1) != CYAN || px(bmp, 12, 6) != CYAN ||
        px(bmp, 13, 1) != BLACK || px(bmp, 0, 1) != BLACK)
        return fail("drawFont6x6 blocks");

    // right edge: clipped STOP, negative x: rows skipped entirely
    bmp.fill(BLACK);
    rt.drawFont6x6(bmp, 60, 1, "ABC", WHITE);
    if (px(bmp, 60, 1) != WHITE || px(bmp, 63, 1) != WHITE)
        return fail("drawFont6x6 right clip");
    bmp.fill(BLACK);
    rt.drawFont6x6(bmp, -2, -2, "A", WHITE);
    if (nonBlack(bmp, 0, 0, 64, 64) != 0)
        return fail("drawFont6x6 negative-x skips rows");

    // drawRect fill vs outline
    bmp.fill(BLACK);
    const rect_t r{.x = 2, .y = 2, .width = 4, .height = 4};
    rt.drawRect(bmp, r, RED, true);
    if (px(bmp, 2, 2) != RED || px(bmp, 5, 5) != RED || px(bmp, 6, 2) != BLACK)
        return fail("drawRect fill");
    bmp.fill(BLACK);
    rt.drawRect(bmp, r, RED, false);
    if (px(bmp, 2, 2) != RED || px(bmp, 3, 3) != BLACK)
        return fail("drawRect outline");

    // BUG-1: overflowing drawFont/drawRect clip instead of writing past the
    // bitmap (previously heap corruption / SIGSEGV)
    bmp.fill(BLACK);
    rt.drawFont(bmp, 60, 62, "ABCDEF", WHITE);
    rt.drawRect(bmp, rect_t{.x = 62, .y = 62, .width = 64, .height = 64}, RED, true);
    if (px(bmp, 62, 62) != RED || px(bmp, 63, 63) != RED)
        return fail("drawRect clipped overflow not drawn");
    bmp.fill(BLACK);
    rt.drawRect(bmp, rect_t{.x = -8, .y = -8, .width = 16, .height = 16}, RED, false);
    if (px(bmp, 7, 0) != RED || px(bmp, 0, 7) != RED || px(bmp, 7, 7) != RED ||
        px(bmp, 0, 0) != BLACK)
        return fail("drawRect negative origin not clipped");

    // drawTile full-blit (const overload)
    CFrame &tile5 = *rt.tiles_()->operator[](5);
    const uint32_t c5 = solidColor(5, 1);
    bmp.fill(BLACK);
    rt.drawTile(bmp, 16, 0, tile5, false);
    if (px(bmp, 16, 0) != c5 || px(bmp, 31, 15) != c5 || px(bmp, 32, 0) != BLACK)
        return fail("drawTile full");

    // drawTile rect overload: sub-rect + transparent skip + color masks
    CFrame patch(16, 16);
    const uint32_t patchColor = solidColor(9, 9);
    std::vector<uint32_t> patchRgb(16 * 16, patchColor);
    patchRgb[0] = CLEAR;
    patch.setRGB(patchRgb);
    bmp.fill(BLACK);
    rt.drawTile(bmp, 8, 8, patch, rect_t{.x = 0, .y = 0, .width = 16, .height = 16});
    if (px(bmp, 8, 8) != BLACK)
        return fail("drawTile rect transparent skip");
    if (px(bmp, 9, 8) != patchColor || px(bmp, 8, 9) != patchColor)
        return fail("drawTile rect blit");
    bmp.fill(BLACK);
    rt.drawTile(bmp, 8, 8, patch, rect_t{.x = 4, .y = 4, .width = 8, .height = 8});
    if (px(bmp, 8, 8) != patchColor || px(bmp, 15, 15) != patchColor ||
        px(bmp, 16, 8) != BLACK)
        return fail("drawTile rect offset");

    struct
    {
        ColorMask mask;
        uint32_t expect;
    } masks[] = {
        {COLOR_FADE, fadedColor(patchColor, 1)},
        {COLOR_INVERTED, patchColor ^ 0x00ffffffu},
        {COLOR_GRAYSCALE, grayColor(patchColor)},
        {COLOR_ALL_WHITE, WHITE},
    };
    for (const auto &m : masks)
    {
        bmp.fill(BLACK);
        rt.drawTile(bmp, 0, 0, patch, rect_t{.x = 1, .y = 0, .width = 4, .height = 4}, m.mask);
        if (px(bmp, 0, 0) != m.expect)
            return fail("drawTile rect colorMask");
    }
    std::unordered_map<uint32_t, uint32_t> cmap{{patchColor, 0xff010203u}};
    bmp.fill(BLACK);
    rt.drawTile(bmp, 0, 0, patch, rect_t{.x = 1, .y = 0, .width = 4, .height = 4},
                COLOR_NOCHANGE, &cmap);
    if (px(bmp, 0, 0) != 0xff010203u)
        return fail("drawTile rect colorMap");

    // drawTileFaz: bit-shift fade + masks
    bmp.fill(BLACK);
    rt.drawTileFaz(bmp, 0, 0, tile5, 0);
    if (px(bmp, 0, 0) != c5)
        return fail("drawTileFaz shift 0");
    bmp.fill(BLACK);
    rt.drawTileFaz(bmp, 0, 0, tile5, 1);
    if (px(bmp, 0, 0) != fadedColor(c5, 1))
        return fail("drawTileFaz fade");
    bmp.fill(BLACK);
    rt.drawTileFaz(bmp, 0, 0, tile5, 2, COLOR_ALL_WHITE);
    if (px(bmp, 0, 0) != WHITE)
        return fail("drawTileFaz all white");
    bmp.fill(BLACK);
    rt.drawTileFaz(bmp, 0, 0, tile5, 1, COLOR_GRAYSCALE);
    if (px(bmp, 0, 0) != fadedColor(grayColor(c5), 1))
        return fail("drawTileFaz gray+fade");

    // plotLine endpoints
    bmp.fill(BLACK);
    rt.plotLine(bmp, 0, 0, 10, 5, WHITE);
    if (px(bmp, 0, 0) != WHITE || px(bmp, 10, 5) != WHITE || px(bmp, 0, 5) != BLACK)
        return fail("plotLine");

    // fazeScreen: white bit-shifts, alpha forced opaque
    bmp.fill(BLACK);
    bmp.getRGB()[1] = WHITE;
    rt.fazeScreen(bmp, 1);
    if (px(bmp, 1, 0) != 0xff7f7f7fu)
        return fail("fazeScreen");

    // flashScreen: keeps alpha, saturates rgb; skips black
    bmp.fill(BLACK);
    uint32_t *rgba = bmp.getRGB().data();
    rgba[64] = 0x12340000u;
    rgba[65] = 0x00123456u;
    rt.flashScreen(bmp);
    if (px(bmp, 0, 1) != 0x12ffffffu || px(bmp, 1, 1) != 0x00ffffffu ||
        px(bmp, 2, 2) != BLACK)
        return fail("flashScreen");

    return true;
}

// --- Phase E: tile/sprite -> frame routing ----------------------------------
bool test_gamemixin_frames_routing()
{
    TestRuntime rt;
    installFixture(rt);
    CGame &game = *CGame::getGame();
    const auto modeBefore = game.mode();
    const int userIDBefore = game.getUserID();
    const uint8_t puBefore = game.playerConst().getPU();
    game.resetStats(); // isolate from stats leaked by earlier suites
    rt.animator_()->reloadTileData(); // NO_ANIMZ baseline for the main layer

    auto call = [&](uint16_t tileID)
    {
        ColorMask mask = COLOR_NOCHANGE;
        std::unordered_map<uint32_t, uint32_t> *map = nullptr;
        CFrame *f = rt.tile2Frame(tileID, mask, map);
        if (mask != COLOR_NOCHANGE || map != nullptr)
            return static_cast<CFrame *>(nullptr); // plain tiles must not tint
        return f;
    };

    if (call(TILES_BLANK) || call(TILES_STOP))
        return fail("blank/stop not skipped");
    if (call(TILES_ZOMBIE))
        return fail("special case not skipped");
    if (call(TILES_BRICKS) != rt.tiles_()->operator[](TILES_BRICKS))
        return fail("plain tile routing");

    // player (ANNIE2) frame selection
    CFrameSet *users = rt.users_().get();
    game.setUserID(1);
    const int base = TestRuntime::PLAYER_TOTAL_FRAMES * 1;
    rt.playerFrameOffset_() = 2;
    if (game.health() <= 0)
        game.addHealth(100);
    game.player().setAim(AIM_UP);

    if (call(TILES_ANNIE2) != users->operator[](AIM_UP * 8 + 2 + base))
        return fail("annie normal aim");

    // closure: no diamonds left + closure flag -> down pose
    if (game.goalCount() == 0)
    {
        game.stats().set(S_CLOSURE, 1);
        if (call(TILES_ANNIE2) != users->operator[](AIM_DOWN * 8 + 2 + base))
            return fail("annie closure");
        game.stats().set(S_CLOSURE, 0);
    }

    // idle animation (aim DOWN)
    game.player().setAim(AIM_DOWN);
    game.stats().set(S_IDLE_TIME, 0x41); // bit 3 clear -> down index
    if (call(TILES_ANNIE2) != users->operator[](TestRuntime::PLAYER_DOWN_INDEX + base))
        return fail("annie idle 0x41");
    game.stats().set(S_IDLE_TIME, 0x58); // bit 3 set -> idle base + ((t>>4)&3)
    const int idleIdx = TestRuntime::PLAYER_IDLE_BASE + ((0x58 >> 4) & 3);
    if (call(TILES_ANNIE2) != users->operator[](idleIdx + base))
        return fail("annie idle 0x58");

    // boat
    game.player().setAim(AIM_UP);
    game.stats().set(S_IDLE_TIME, 0);
    game.stats().set(S_BOAT, 1);
    game.player().setPU(TILES_SWAMP);
    if (call(TILES_ANNIE2) != users->operator[](TestRuntime::PLAYER_BOAT_FRAME + base))
        return fail("annie boat");
    game.resetStats();
    game.player().setPU(puBefore);

    // hurt color masks
    game.stats().set(S_PLAYER_HURT, CGame::HurtFlash);
    {
        ColorMask mask = COLOR_NOCHANGE;
        std::unordered_map<uint32_t, uint32_t> *map = nullptr;
        rt.tile2Frame(TILES_ANNIE2, mask, map);
        if (mask != COLOR_ALL_WHITE)
            return fail("hurt flash mask");
        game.stats().set(S_PLAYER_HURT, CGame::HurtInv);
        rt.tile2Frame(TILES_ANNIE2, mask, map);
        if (mask != COLOR_INVERTED)
            return fail("hurt inv mask");
        game.stats().set(S_PLAYER_HURT, CGame::HurtFaz);
        rt.tile2Frame(TILES_ANNIE2, mask, map);
        if (mask != COLOR_FADE)
            return fail("hurt faz mask");
        // freeze wins over hurt
        game.stats().set(S_FREEZE_TIMER, 1);
        rt.tile2Frame(TILES_ANNIE2, mask, map);
        if (mask != COLOR_GRAYSCALE || map != nullptr)
            return fail("freeze mask");
        game.resetStats(); // now clears S_PLAYER_HURT / S_BOAT too
    }

    // color maps
    game.stats().set(S_EXTRA_SPEED_TIMER, 1);
    {
        ColorMask mask = COLOR_NOCHANGE;
        std::unordered_map<uint32_t, uint32_t> *map = nullptr;
        rt.tile2Frame(TILES_ANNIE2, mask, map);
        if (map != &rt.colormaps_().sugarRush)
            return fail("sugar rush map");
        game.resetStats();
        game.stats().set(S_GOD_MODE_TIMER, 1);
        rt.tile2Frame(TILES_ANNIE2, mask, map);
        if (map != &rt.colormaps_().godMode)
            return fail("god mode map");
        game.resetStats();
        game.stats().set(S_RAGE_TIMER, 1);
        rt.tile2Frame(TILES_ANNIE2, mask, map);
        if (map != &rt.colormaps_().rage)
            return fail("rage map");
        game.resetStats();
    }

    // dead player: tile2Frame keys off health()==0 (killPlayer only spends lives)
    game.player().setAim(AIM_UP);
    game.addHealth(-1000);
    if (call(TILES_ANNIE2) !=
        users->operator[](TestRuntime::INDEX_PLAYER_DEAD * 8 + 2 + base))
        return fail("annie dead");
    game.addHealth(1000);

    // calcSpecialFrame
    CFrameSet *tiles = rt.tiles_().get();
    CFrameSet *animz = rt.animz_().get();
    TestRuntime::sprite_t sp{};
    sp.tileID = TILES_ZOMBIE;
    sp.aim = AIM_UP;
    sp.attr = ATTR_IDLE_MIN + 2;
    if (rt.calcSpecialFrame(sp) != tiles->operator[](TILES_ZOMBIE))
        return fail("special idle attr frame");
    sp.attr = 0;
    sp.frame = 3;
    sp.tileID = TILES_EGG_WHOLE;
    if (rt.calcSpecialFrame(sp) != animz->operator[](ANIMZ_EGG_HATCH + 3))
        return fail("egg hatch frame");

    sp.tileID = TILES_ZOMBIE;
    sp.aim = AIM_RIGHT;
    sp.frame = 0;
    {
        const auto info = rt.animator_()->getSpecialInfo(TILES_ZOMBIE);
        const int idx = (int)AIM_RIGHT * info.frames + info.base + info.offset;
        if (rt.calcSpecialFrame(sp) != animz->operator[](idx))
            return fail("zombie special frame");
    }

    // TYPE_DRONE tiles fold the aim to a single bit
    {
        int droneTile = -1;
        for (int t = 0; t < TILES_TOTAL_COUNT; ++t)
            if (getTileDef(t).type == TYPE_DRONE)
            {
                droneTile = t;
                break;
            }
        if (droneTile >= 0)
        {
            sp.tileID = static_cast<uint16_t>(droneTile);
            sp.aim = AIM_DOWN; // &= 1 -> 1
            const auto info = rt.animator_()->getSpecialInfo(droneTile);
            const int idx = 1 * info.frames + info.base + info.offset;
            if (rt.calcSpecialFrame(sp) != animz->operator[](idx))
                return fail("drone aim fold");
        }
    }

    // gatherSprites: monsters on special tiles + sfx inside the viewport
    rt.setWidth(64);
    rt.setHeight(48);
    CMap &map = game.getMap();
    map.clear();
    map.resize(10, 10, TILES_BLANK, true);
    map.getMainLayer()->set(2, 2, TILES_ZOMBIE);
    map.getMainLayer()->set(5, 5, TILES_ZOMBIE);
    auto &monsters = game.getMonsters();
    monsters.clear();
    monsters.emplace_back(2, 2, TILES_ZOMBIE); // in viewport, special
    monsters.emplace_back(5, 5, TILES_ZOMBIE); // out of viewport
    auto &sfx = game.getSfx();
    sfx.clear();
    sfx.push_back({3, 1, static_cast<Sfx>(MIN_SFX + 1), 10}); // inside
    sfx.push_back({9, 9, static_cast<Sfx>(MIN_SFX + 2), 10}); // outside

    std::vector<TestRuntime::sprite_t> sprites;
    const TestRuntime::cameraContext_t ctx{0, 0, 0, 0};
    rt.gatherSprites(sprites, ctx);
    monsters.clear();
    sfx.clear();
    if (sprites.size() != 2)
        return fail("gather sprite count");
    if (sprites[0].tileID != TILES_ZOMBIE || sprites[0].x != 2 || sprites[0].y != 2)
        return fail("gather monster sprite");
    if (sprites[1].aim != AIM_NONE || sprites[1].x != 3)
        return fail("gather sfx sprite");

    game.resetStats();
    game.setUserID(userIDBefore);
    game.setMode(modeBefore);
    return true;
}

// --- Phase E: viewports, camera, overlay widgets ----------------------------
bool test_gamemixin_viewports()
{
    TestRuntime rt;
    installFixture(rt);
    CGame &game = *CGame::getGame();
    CMap &map = game.getMap();
    const uint16_t timeoutBefore = map.statesConst().getU(TIMEOUT);
    rt.animator_()->reloadTileData();
    rt.setWidth(64);
    rt.setHeight(64); // 4x4 tiles
    map.clear();
    map.resize(10, 10, TILES_BLANK, true);
    map.getMainLayer()->set(0, 0, TILES_BRICKS);
    map.getMainLayer()->set(1, 1, TILES_BRICKS_2);
    game.getMonsters().clear();
    game.getSfx().clear();
    game.getBosses().clear();
    game.player().setPos(Pos{0, 0});

    CFrame bmp(64, 64);

    // static camera at map origin
    rt.setCameraMode(TestRuntime::CAMERA_MODE_STATIC);
    rt.drawViewPortStatic(bmp);
    if (px(bmp, 8, 8) != solidColor(TILES_BRICKS, 1))
        return fail("static viewport origin tile");
    if (px(bmp, 24, 24) != solidColor(TILES_BRICKS_2, 1))
        return fail("static viewport second tile");
    if (px(bmp, 48, 48) != BLACK)
        return fail("static viewport blank tile");

    // centerCamera follows the player, clamped to map bounds
    rt.centerCamera();
    if (rt.camX() != 0 || rt.camY() != 0)
        return fail("centerCamera origin");
    game.player().setPos(Pos{9, 9});
    map.getMainLayer()->set(6, 6, TILES_BRICKS_3);
    rt.centerCamera(); // mx = min(9-2, 10-4) = 6
    if (rt.camX() != 12 || rt.camY() != 12)
        return fail("centerCamera clamp");
    rt.drawViewPortStatic(bmp);
    if (px(bmp, 0, 0) != solidColor(TILES_BRICKS_3, 1))
        return fail("static viewport scrolled");

    // monster on a special tile routes through calcSpecialFrame
    game.player().setPos(Pos{0, 0});
    rt.camX() = 0;
    rt.camY() = 0;
    map.getMainLayer()->set(2, 2, TILES_ZOMBIE);
    auto &monsters = game.getMonsters();
    monsters.clear();
    monsters.emplace_back(2, 2, TILES_ZOMBIE);
    game.rebuildMonsterGrid();
    {
        const auto info = rt.animator_()->getSpecialInfo(TILES_ZOMBIE);
        const int idx = (int)monsters[0].getAim() * info.frames + info.base + info.offset;
        rt.drawViewPortStatic(bmp);
        if (px(bmp, 32, 32) != solidColor(idx, 2))
            return fail("static viewport monster frame");
    }
    monsters.clear();
    game.rebuildMonsterGrid();

    // dynamic camera: even and odd sub-tile offsets
    game.player().setPos(Pos{9, 9}); // keeps map(6,6) content in view
    rt.setCameraMode(TestRuntime::CAMERA_MODE_DYNAMIC);
    rt.camX() = 12;
    rt.camY() = 12;
    rt.drawViewPortDynamic(bmp);
    if (px(bmp, 0, 0) != solidColor(TILES_BRICKS_3, 1))
        return fail("dynamic viewport even offset");
    if (px(bmp, 16, 0) != BLACK)
        return fail("dynamic viewport blank");
    // Odd camera offsets (ox=1) probe one column past the window.
    map.getMainLayer()->set(5, 6, TILES_BRICKS_2);
    rt.camX() = 11; // mx = 5, ox = 1: first column half-clipped at x=0
    rt.drawViewPortDynamic(bmp);
    if (px(bmp, 0, 0) != solidColor(TILES_BRICKS_2, 1) ||
        px(bmp, 8, 0) != solidColor(TILES_BRICKS_3, 1))
        return fail("dynamic viewport odd offset");
    map.getMainLayer()->set(5, 6, TILES_BLANK);
    // BUG-3: flush half-pixel window to the map edge: the over-wide half
    // column/row is clipped, map->at(width) is never read (used to throw
    // std::out_of_range).
    map.getMainLayer()->set(9, 9, TILES_BRICKS_2);
    rt.camX() = 13; // mx = 6, ox = 1: x == cols would read at(10)
    rt.camY() = 13;
    bmp.fill(BLACK);
    rt.drawViewPortDynamic(bmp);
    if (px(bmp, 40, 40) != solidColor(TILES_BRICKS_2, 1))
        return fail("dynamic viewport edge tile missing");
    if (px(bmp, 56, 40) != BLACK)
        return fail("dynamic viewport edge half-column drawn");
    map.getMainLayer()->set(9, 9, TILES_BLANK);
    rt.camX() = 12;
    rt.camY() = 12;
    rt.setCameraMode(TestRuntime::CAMERA_MODE_STATIC);

    // drawScroll: left/mid/right uisheet parts along the bottom
    bmp.fill(BLACK);
    rt.drawScroll(bmp);
    if (px(bmp, 0, 16) != solidColor(0, 4) || px(bmp, 16, 16) != solidColor(1, 4) ||
        px(bmp, 48, 16) != solidColor(2, 4) || px(bmp, 0, 15) != BLACK)
        return fail("drawScroll");

    // drawTimeout
    rt.setWidth(160);
    map.states().setU(TIMEOUT, 100);
    rt.ticks() = 0;
    bmp.fill(BLACK); // 64 wide but 160 px text would overrun: use wide bitmap
    CFrame wide(160, 64);
    wide.fill(BLACK);
    rt.drawTimeout(wide);
    // "%.2d" of 99, scaleX 3 -> x = 160 - 3*8*2 - 8 = 104
    if (px(wide, 104, 16) != YELLOW)
        return fail("drawTimeout yellow");
    map.states().setU(TIMEOUT, 15); // low time: scale 5, blinks ORANGE
    rt.ticks() = 8;
    wide.fill(BLACK);
    rt.drawTimeout(wide); // x = 160 - 5*8*2 - 8 = 72, text "14"
    if (px(wide, 72, 16) != ORANGE)
        return fail("drawTimeout low-time orange");
    map.states().setU(TIMEOUT, 0);
    wide.fill(BLACK);
    rt.drawTimeout(wide);
    if (nonBlack(wide, 0, 0, 160, 64) != 0)
        return fail("drawTimeout zero no-op");
    map.states().setU(TIMEOUT, timeoutBefore);

    // drawKeys: rightmost first, MAX_KEY_STATE flashes all-white
    auto &keys = CGame::keys();
    memset(keys.tiles, 0, sizeof(keys.tiles));
    memset(keys.indicators, 0, sizeof(keys.indicators));
    keys.tiles[0] = TILES_KEY01;
    keys.indicators[0] = CGame::MAX_KEY_STATE;
    keys.tiles[1] = TILES_KEY02;
    keys.indicators[1] = 1;
    rt.setWidth(64);
    bmp.fill(BLACK);
    rt.ticks() = 0;
    rt.drawKeys(bmp);
    if (px(bmp, 48, 48) != WHITE)
        return fail("drawKeys all-white key");
    if (px(bmp, 32, 48) != fadedColor(solidColor(TILES_KEY02, 1), 1))
        return fail("drawKeys faded key");
    if (px(bmp, 16, 48) != BLACK)
        return fail("drawKeys empty slot");
    rt.ticks() = 2; // (ticks >> 1) & 1 -> indicator decay after drawing
    bmp.fill(BLACK);
    rt.drawKeys(bmp);
    if (keys.indicators[0] != CGame::MAX_KEY_STATE - 1 || keys.indicators[1] != 0)
        return fail("drawKeys indicator decay");
    if (px(bmp, 48, 48) != WHITE) // u was still MAX at draw time: decay runs after
        return fail("drawKeys all-white second frame");
    rt.ticks() = 0; // stop further decay, render the decayed state
    bmp.fill(BLACK);
    rt.drawKeys(bmp);
    if (px(bmp, 48, 48) != fadedColor(solidColor(TILES_KEY01, 1), 4))
        return fail("drawKeys decayed key");
    memset(keys.tiles, 0, sizeof(keys.tiles));
    memset(keys.indicators, 0, sizeof(keys.indicators));

    map.getMainLayer()->set(2, 2, TILES_BLANK);
    return true;
}

// --- Phase E: status bar, health bar, screen composition, UI ----------------
bool test_gamemixin_screen()
{
    TestRuntime rt;
    installFixture(rt);
    CGame &game = *CGame::getGame();
    CMap &map = game.getMap();
    rt.animator_()->reloadTileData();
    map.clear();
    map.resize(10, 10, TILES_BLANK, true);
    map.getMainLayer()->set(0, 0, TILES_BRICKS);
    game.getMonsters().clear();
    game.getSfx().clear();
    game.getBosses().clear();
    game.player().setPos(Pos{0, 0});
    if (game.health() <= 0)
        game.addHealth(100);

    // status bar (width < MIN_WIDTH_FULL keeps the sugar meter out)
    rt.setWidth(240);
    rt.setHeight(64);
    CFrame sbmp(240, 64);
    rt.paused_() = true;
    sbmp.fill(BLACK);
    rt.drawGameStatus(sbmp, {});
    if (px(sbmp, 0, 2) != LIGHTGRAY)
        return fail("status paused banner");
    rt.paused_() = false;
    rt.prompt_() = TestRuntime::PROMPT_SAVE;
    sbmp.fill(BLACK);
    rt.drawGameStatus(sbmp, {});
    if (px(sbmp, 0, 2) != LIGHTGRAY)
        return fail("status prompt banner");
    rt.prompt_() = TestRuntime::PROMPT_NONE;
    sbmp.fill(BLACK);
    rt.drawGameStatus(sbmp, {});
    if (px(sbmp, 0, 2) != WHITE)
        return fail("status score color");
    if (px(sbmp, 72, 2) != YELLOW)
        return fail("status diamonds color");
    if (px(sbmp, 168, 2) != PURPLE) // (9 + strlen("DIAMONDS %.2d ")) * 8
        return fail("status lives color");
    // shimmer cues: previous counters differ from the live values
    rt.visual_().rGoalCount = game.goalCount() + 1;
    rt.visual_().rLives = game.lives() - 1;
    const TestRuntime::visualCues_t cues{.diamondShimmer = game.goalCount() < rt.visual_().rGoalCount,
                            .livesShimmer = game.lives() > rt.visual_().rLives};
    sbmp.fill(BLACK);
    rt.drawGameStatus(sbmp, cues);
    if (px(sbmp, 72, 2) != DEEPSKYBLUE)
        return fail("status diamond shimmer");
    if (px(sbmp, 168, 2) != GREEN)
        return fail("status lives shimmer");

    // sugar meter clamps out-of-range S_SUGAR (crafted save) to the last cube
    CGame &gs = *CGame::getGame();
    gs.stats().set(S_SUGAR, 200);
    rt.visual_().rSugar = 0;
    rt.visual_().sugarFx = 0;
    memset(rt.visual_().sugarCubes, 0, sizeof(rt.visual_().sugarCubes));
    CFrame meter(120, 64);
    rt.sugarMeter_(meter, 0);
    auto &cubes = rt.visual_().sugarCubes;
    const size_t lastCube = sizeof(cubes) / sizeof(cubes[0]) - 1;
    if (!rt.visual_().sugarFx || cubes[lastCube] != rt.visual_().sugarFx)
        return fail("S_SUGAR=200 not clamped to last cube");
    for (size_t i = 0; i < lastCube; ++i)
        if (cubes[i] != 0)
            return fail("clamp wrote wrong cube");
    gs.stats().set(S_SUGAR, 0);
    rt.visual_().sugarFx = 0;
    memset(rt.visual_().sugarCubes, 0, sizeof(rt.visual_().sugarCubes));
    rt.visual_().rSugar = 0;

    // classic health bar
    CFrame hb(240, 64);
    rt.healthBar_() = TestRuntime::HEALTHBAR_CLASSIC;
    hb.fill(BLACK);
    rt.drawHealthBar(hb, false);
    if (px(hb, 4, 52) != WHITE) // outline corner
        return fail("healthbar outline");
    if (px(hb, 5, 53) != LIME)
        return fail("healthbar fill");
    hb.fill(BLACK);
    rt.drawHealthBar(hb, true);
    if (px(hb, 5, 53) != PINK)
        return fail("healthbar hurt color");
    game.stats().set(S_GOD_MODE_TIMER, 1);
    hb.fill(BLACK);
    rt.drawHealthBar(hb, false);
    if (px(hb, 5, 53) != WHITE)
        return fail("healthbar god mode");
    game.resetStats();

    // hearts variant
    if (game.maxHealth() >= 16 && game.health() >= 8)
    {
        rt.healthBar_() = TestRuntime::HEALTHBAR_HEARTHS;
        hb.fill(BLACK);
        rt.drawHealthBar(hb, false);
        if (px(hb, 2, 52) != RED)
            return fail("healthbar heart");
        hb.fill(BLACK);
        rt.drawHealthBar(hb, true);
        if (px(hb, 2, 52) != PINK)
            return fail("healthbar hurt heart");
        rt.healthBar_() = TestRuntime::HEALTHBAR_CLASSIC;
    }

    // drawScreen composition (msg scroll + flash + hurt + UI visible)
    rt.setWidth(112);
    rt.setHeight(104);
    CFrame fbmp(112, 104);
    const int eventBefore = rt.event();
    const uint16_t timeoutBefore2 = map.statesConst().getU(TIMEOUT);
    const std::string msg0Before = map.states().getS(MSG0);
    map.states().setS(MSG0, "ALPHA\nBETA");
    map.states().setU(TIMEOUT, 0);
    rt.event() = MSG0;
    game.stats().set(S_FLASH, 1);
    game.stats().set(S_PLAYER_HURT, CGame::HurtFlash);
    rt.ui_().show();
    rt.drawScreen(fbmp);
    game.stats().set(S_FLASH, 0);
    game.stats().set(S_PLAYER_HURT, CGame::HurtNone);
    if (nonBlack(fbmp, 0, 0, 112, 104) < 100)
        return fail("drawScreen composition empty");

    // EVENT_SUGAR bottom rectangle
    rt.event() = EVENT_SUGAR;
    fbmp.fill(BLACK);
    rt.drawScreen(fbmp);
    if (px(fbmp, 1, 104 - 16 + 1) != DARKGRAY)
        return fail("sugar event rect");
    if (px(fbmp, 0, 104 - 16) != LIGHTGRAY)
        return fail("sugar event border");

    // drawUI button grid (note: drawUI positions by getWidth for both axes)
    const int baseX = 112 - 96 - 8;
    const int baseYdraw = 112 - 64 - 8;
    if (px(fbmp, baseX + 32, baseYdraw) != RED) // border over the white fill
        return fail("drawUI button border");
    if (px(fbmp, baseX + 33, baseYdraw + 1) != WHITE)
        return fail("drawUI button fill");

    // whichButton maps clicks to button ids (uses the correct getHeight)
    if (rt.ui_().buttons().size() != 4)
        return fail("ui button count");
    if (rt.whichButton(rt.ui_(), baseX + 32 + 10, 104 - 64 - 8 + 10) != AIM_UP)
        return fail("whichButton up");
    if (rt.whichButton(rt.ui_(), baseX + 10, 104 - 64 - 8 + 40) != AIM_LEFT)
        return fail("whichButton left");
    if (rt.whichButton(rt.ui_(), 0, 0) != TestRuntime::INVALID)
        return fail("whichButton miss");
    rt.ui_().hide();

    rt.event() = eventBefore;
    map.states().setU(TIMEOUT, timeoutBefore2);
    map.states().setS(MSG0, msg0Before);
    return true;
}

// --- Phase E: full-screen overlays ------------------------------------------
bool test_gamemixin_screens()
{
    TestRuntime rt;
    installFixture(rt); // drawFont needs the synthetic font atlas
    const int widthBefore = rt.width_();
    const int heightBefore = rt.height_();

    // drawScores fixture 304 wide (drawFont now clips, but centred strings
    // are laid out for this width)
    rt.setWidth(304);
    rt.setHeight(208);
    CFrame bmp(304, 208);
    rt.clearScores();
    rt.scoreRank_() = 3;
    rt.recordScore_() = false;
    rt.ticks() = 0;
    rt.drawScores(bmp);
    if (px(bmp, 40, 8) != WHITE || px(bmp, 40, 24) != WHITE)
        return fail("scores header");
    if (px(bmp, 1, 40) != DARKGRAY) // rows 0/1 dark, 2/3 light
        return fail("scores row 0 color");
    if (px(bmp, 1, 56) != LIGHTGRAY)
        return fail("scores row 2 color");
    if (px(bmp, 1, 64) != CYAN) // ranked row (not recording)
        return fail("scores rank color");
    rt.recordScore_() = true;
    rt.drawScores(bmp);
    if (px(bmp, 1, 64) != YELLOW)
        return fail("scores recording row yellow");
    if (px(bmp, 0, 192) != YELLOW) // "PLEASE TYPE YOUR NAME..." footer
        return fail("scores name prompt");
    rt.ticks() = TestRuntime::CARET_SPEED; // caret branch (glyph appended)
    rt.drawScores(bmp);
    if (px(bmp, 1, 64) != YELLOW)
        return fail("scores caret row");
    rt.recordScore_() = false;
    rt.scoreRank_() = TestRuntime::INVALID;
    rt.drawScores(bmp);
    if (px(bmp, 0, 192) != YELLOW) // "SORRY, YOU DIDN'T QUALIFY." footer
        return fail("scores disqualify footer");

    // drawHelpScreen: '~' headers, '!' centred lines
    auto &help = rt.helptext();
    help = {"~HEAD", "plain", "!CENTER"};
    rt.setWidth(112);
    rt.setHeight(32);
    CFrame hb(112, 32);
    rt.drawHelpScreen(hb);
    if (px(hb, 0, 0) != YELLOW)
        return fail("help header line");
    if (px(hb, 0, 8) != WHITE)
        return fail("help plain line");
    if (px(hb, 32, 16) != WHITE || px(hb, 0, 16) != BLACK)
        return fail("help centered line");
    help.clear();

    // drawLevelIntro per game mode
    const auto modeBefore = rt.event();
    const int timerBefore = rt.timer();
    CGame &game = *CGame::getGame();
    const auto gameModeBefore = game.mode();
    const int livesBefore = game.lives();
    game.getMap().setTitle("");
    rt.setWidth(256);
    rt.setHeight(104);
    CFrame lb(256, 104);
    struct
    {
        CGame::GameMode mode;
        int lives;
        int x;
    } intros[] = {
        {CGame::MODE_LEVEL_INTRO, 2, 64},   // "LEVEL 01"
        {CGame::MODE_RESTART, 2, 24},       // "LIVES LEFT 02"
        {CGame::MODE_RESTART, 1, 40},       // "LAST LIFE !"
        {CGame::MODE_GAMEOVER, 1, 80},      // "GAME OVER"
        {CGame::MODE_TIMEOUT, 1, 40},       // "OUT OF TIME"
        {CGame::MODE_CHUTE, 1, 0},          // "DROP TO LEVEL 01"
    };
    for (const auto &intro : intros)
    {
        game.setMode(intro.mode);
        game.setLives(intro.lives);
        rt.event() = 12345; // reset to EVENT_NONE by drawLevelIntro
        lb.fill(BLACK);
        rt.drawLevelIntro(lb);
        if (px(lb, intro.x, 44) != YELLOW)
            return fail("level intro banner");
        if (rt.event() != EVENT_NONE || rt.timer() != TestRuntime::TICK_RATE)
            return fail("level intro state reset");
    }

    // drawPreScreen
    rt.setWidth(112);
    CFrame pb(112, 104);
    rt.drawPreScreen(pb);
    if (px(pb, 0, 48) != WHITE)
        return fail("pre screen text");

    // drawEventText (bottom-anchored, colour per event)
    const int eventBefore2 = rt.event();
    rt.setWidth(128);
    CFrame eb(128, 104);
    eb.fill(BLACK);
    rt.event() = EVENT_SECRET;
    rt.drawEventText(eb);
    if (px(eb, 0, 104 - 4 - 8) != CYAN) // "SECRET !" scale 2, baseY = h-4
        return fail("event text secret");
    eb.fill(BLACK);
    rt.event() = EVENT_NONE;
    rt.drawEventText(eb);
    if (nonBlack(eb, 0, 0, 128, 104) != 0)
        return fail("event text none");
    rt.event() = eventBefore2;

    game.setMode(gameModeBefore);
    game.setLives(livesBefore);
    rt.event() = static_cast<int>(modeBefore);
    rt.timer() = timerBefore;
    rt.setWidth(widthBefore);
    rt.setHeight(heightBefore);
    return true;
}

// --- Phase E: keyboard input rules -------------------------------------------
bool test_gamemixin_input()
{
    TestRuntime rt;
    CGame &game = *CGame::getGame();
    const auto modeBefore = game.mode();

    // handleInputString: key repeater + charset + backspace + enter
    char buf[10] = {0};
    rt.clearKeyStates();
    rt.keyState_(TestRuntime::Key_A) = 1;
    if (rt.handleInputString(buf, sizeof(buf)) || strcmp(buf, "A") != 0)
        return fail("input first char");
    if (rt.keyRepeter_(TestRuntime::Key_A) != TestRuntime::KEY_REPETE_DELAY)
        return fail("input repeter armed");
    // held key only repeats after KEY_REPETE_DELAY further polls
    for (int i = 0; i < static_cast<int>(TestRuntime::KEY_REPETE_DELAY) - 1; ++i)
        rt.handleInputString(buf, sizeof(buf));
    if (strcmp(buf, "A") != 0)
        return fail("input repeter held");
    rt.handleInputString(buf, sizeof(buf));
    if (strcmp(buf, "AA") != 0)
        return fail("input repeat");
    rt.keyState_(TestRuntime::Key_A) = 0;
    rt.clearKeyStates();

    strcpy(buf, "123456789"); // limit - 1
    rt.keyState_(TestRuntime::Key_A) = 1;
    rt.handleInputString(buf, sizeof(buf));
    if (strcmp(buf, "123456789") != 0)
        return fail("input maxlength");
    rt.keyState_(TestRuntime::Key_A) = 0;
    rt.keyState_(TestRuntime::Key_F1) = 1; // unhandled key: no char, no repeat
    rt.handleInputString(buf, sizeof(buf));
    if (strcmp(buf, "123456789") != 0 || rt.keyRepeter_(TestRuntime::Key_F1) != 0)
        return fail("input unhandled key");
    rt.keyState_(TestRuntime::Key_F1) = 0;

    buf[0] = 0;
    rt.keyState_(TestRuntime::Key_0) = 1;
    rt.keyState_(TestRuntime::Key_9) = 1;
    rt.keyState_(TestRuntime::Key_Period) = 1;
    rt.keyState_(TestRuntime::Key_Space) = 1;
    rt.handleInputString(buf, sizeof(buf));
    if (strcmp(buf, " 09.") != 0) // scan order: Space < Key_0 < Key_9 < Period
        return fail("input charset");
    rt.clearKeyStates();
    rt.keyState_(TestRuntime::Key_BackSpace) = 1;
    rt.handleInputString(buf, sizeof(buf));
    if (strcmp(buf, " 09") != 0)
        return fail("input backspace");
    rt.keyState_(TestRuntime::Key_BackSpace) = 0;
    rt.keyState_(TestRuntime::Key_Enter) = 1;
    if (!rt.handleInputString(buf, sizeof(buf)))
        return fail("input enter");
    rt.clearKeyStates();

    // handleFunctionKeys_Game: prompts, pause, help
    game.setMode(CGame::MODE_PLAY);
    rt.prompt_() = TestRuntime::PROMPT_NONE;
    rt.handleFunctionKeys_Game(TestRuntime::Key_F2);
    if (rt.prompt_() != TestRuntime::PROMPT_RESTART_GAME)
        return fail("F2 prompt");
    rt.prompt_() = TestRuntime::PROMPT_NONE;
    rt.handleFunctionKeys_Game(TestRuntime::Key_F3);
    if (rt.prompt_() != TestRuntime::PROMPT_ERASE_SCORES)
        return fail("F3 prompt");
    rt.handleFunctionKeys_Game(TestRuntime::Key_F9);
    if (rt.prompt_() != TestRuntime::PROMPT_LOAD)
        return fail("F9 prompt");
    rt.handleFunctionKeys_Game(TestRuntime::Key_F10);
    if (rt.prompt_() != TestRuntime::PROMPT_SAVE)
        return fail("F10 prompt");
    rt.handleFunctionKeys_Game(TestRuntime::Key_F11);
    if (rt.prompt_() != TestRuntime::PROMPT_TOGGLE_MUSIC)
        return fail("F11 prompt");
    rt.handleFunctionKeys_Game(TestRuntime::Key_F12);
    if (rt.prompt_() != TestRuntime::PROMPT_HARDCORE)
        return fail("F12 prompt");
    rt.prompt_() = TestRuntime::PROMPT_NONE;
    if (rt.paused_())
        return fail("paused initial state");
    rt.handleFunctionKeys_Game(TestRuntime::Key_F4);
    if (!rt.paused_())
        return fail("F4 pause");
    rt.handleFunctionKeys_Game(TestRuntime::Key_F4);
    if (rt.paused_())
        return fail("F4 resume");
    {
        game.setMode(CGame::MODE_IDLE);
        rt.handleFunctionKeys_Game(TestRuntime::Key_F1);
        if (game.mode() != CGame::MODE_HELP)
            return fail("F1 help mode");
        if (rt.keyRepeter_(TestRuntime::Key_F1) != TestRuntime::KEY_NO_REPETE)
            return fail("F1 repeter");
        rt.keyRepeter_(TestRuntime::Key_F1) = 0;
    }

    // handleFunctionKeys loop: mode dispatch, pause gate, keyup reset
    rt.clearKeyStates();
    game.setMode(CGame::MODE_PLAY);
    rt.keyState_(TestRuntime::Key_F3) = 1;
    rt.handleFunctionKeys();
    if (rt.prompt_() != TestRuntime::PROMPT_ERASE_SCORES)
        return fail("loop F3 prompt");
    rt.prompt_() = TestRuntime::PROMPT_NONE;
    rt.keyState_(TestRuntime::Key_F3) = 0;
    rt.keyState_(TestRuntime::Key_F1) = 1;
    rt.handleFunctionKeys();
    if (game.mode() != CGame::MODE_HELP)
        return fail("loop F1 mode");
    game.setMode(CGame::MODE_PLAY);
    rt.handleFunctionKeys(); // KEY_NO_REPETE blocks the held key
    if (rt.keyState_(TestRuntime::Key_F1) != 1)
        return fail("loop keyup cleared too early");
    rt.keyState_(TestRuntime::Key_F1) = 0; // keyup resets the repeter
    rt.handleFunctionKeys();
    if (rt.keyRepeter_(TestRuntime::Key_F1) != 0)
        return fail("loop keyup repeter reset");
    rt.paused_() = true;
    rt.keyState_(TestRuntime::Key_F3) = 1;
    rt.handleFunctionKeys(); // hotkeys ignored while paused
    if (rt.prompt_() != TestRuntime::PROMPT_NONE)
        return fail("loop pause gate");
    rt.keyState_(TestRuntime::Key_F3) = 0;
    rt.keyState_(TestRuntime::Key_F4) = 1;
    rt.handleFunctionKeys(); // except F4
    if (rt.paused_())
        return fail("loop F4 while paused");
    rt.keyState_(TestRuntime::Key_F4) = 0;
    game.setMode(CGame::MODE_TITLE);
    rt.keyState_(TestRuntime::Key_F3) = 1;
    rt.handleFunctionKeys(); // general mode ignores game hotkeys
    if (rt.prompt_() != TestRuntime::PROMPT_NONE)
        return fail("loop general dispatch");
    rt.clearKeyStates();

    // handlePrompts
    rt.prompt_() = TestRuntime::PROMPT_NONE;
    if (rt.handlePrompts())
        return fail("prompt none");
    rt.prompt_() = TestRuntime::PROMPT_ERASE_SCORES;
    rt.keyState_(TestRuntime::Key_N) = 1;
    if (!rt.handlePrompts() || rt.prompt_() != TestRuntime::PROMPT_NONE)
        return fail("prompt cancel");
    rt.clearKeyStates();
    const int livesBefore = game.lives();
    game.setLives(5);
    rt.prompt_() = TestRuntime::PROMPT_HARDCORE;
    rt.keyState_(TestRuntime::Key_Y) = 1;
    if (!rt.handlePrompts() || game.lives() != 1)
        return fail("prompt hardcore");
    rt.clearKeyStates();
    game.setLives(livesBefore);
    // only the un-mute direction: stopMusic() is null-guarded
    rt.musicMuted_() = 1;
    rt.prompt_() = TestRuntime::PROMPT_TOGGLE_MUSIC;
    rt.keyState_(TestRuntime::Key_Y) = 1;
    rt.handlePrompts();
    if (rt.musicMuted_() != 0)
        return fail("prompt toggle music");
    rt.clearKeyStates();
    rt.prompt_() = TestRuntime::PROMPT_NONE;

    // misc state setters
    rt.enableHiScore();
    if (!rt.hiscoreEnabled_())
        return fail("enableHiScore");
    rt.startCountdown(3);
    if (rt.countdown_() != 3 * TestRuntime::INTRO_DELAY)
        return fail("startCountdown");
    const uint8_t skillBefore = game.skill();
    rt.setSkill(2);
    if (game.skill() != 2)
        return fail("setSkill");
    rt.setSkill(skillBefore);
    rt.setCameraMode(2); // unknown mode falls back to static
    if (rt.cameraMode_() != TestRuntime::CAMERA_MODE_STATIC)
        return fail("setCameraMode fallback");
    rt.setCameraMode(TestRuntime::CAMERA_MODE_DYNAMIC);
    if (rt.cameraMode_() != TestRuntime::CAMERA_MODE_DYNAMIC)
        return fail("setCameraMode dynamic");
    rt.setCameraMode(TestRuntime::CAMERA_MODE_STATIC);
    const int expectedSpeed = std::max(game.playerSpeed() - 1, 1);
    if (rt.cameraSpeed() != expectedSpeed)
        return fail("cameraSpeed"); // stopRecorder is private and starts stopped: untested

    game.setMode(modeBefore);
    return true;
}

// --- Phase E: boss rendering --------------------------------------------------
bool test_gamemixin_bosses()
{
    constexpr const char *INI_DIR = "data/bosses";
    if (!fs::exists(INI_DIR))
        return true; // data not deployed: skip (same policy as t_boss)
    if (g_bossCount == 0 && loadBossData(INI_DIR, "data/bosses/data") != 0)
        return true;
    // BUG-C8: a frame number outside the sheet is skipped, not dereferenced.
    // Synthesized copy keeps the case independent of real sheet ids:
    // currentFrame() == 0 against an emptied sheet0.
    if (g_bossCount > 0)
    {
        bossData_t synth = g_bossData[0];
        synth.sheet = 0;
        synth.aims = 1;
        synth.moving.base = 0;
        synth.moving.lenght = 1;
        TestRuntime rt;
        installFixture(rt);
        CGame &game = *CGame::getGame();
        auto &bosses = game.getBosses();
        bosses.clear();
        bosses.emplace_back(4, 4, &synth);
        rt.sheet0_() = std::make_unique<CFrameSet>(); // no frames at all
        CFrame bmp(64, 64);
        bmp.fill(BLACK);
        rt.drawBossses(bmp, 0, 0, 8, 8); // pre-fix: nullptr frame deref
        if (nonBlack(bmp, 0, 0, bmp.width(), bmp.height()) != 0)
        {
            bosses.clear();
            return fail("out-of-range boss frame was drawn");
        }
        bosses.clear();
    }

    const bossData_t *data = nullptr;
    for (int i = 0; i < g_bossCount; ++i)
    {
        const bossData_t *d = getBossData(static_cast<uint8_t>(i));
        if (d && (d->sheet == 0 || d->sheet == 1))
        {
            data = d;
            break;
        }
    }
    if (!data)
        return true;

    TestRuntime rt;
    installFixture(rt);
    CGame &game = *CGame::getGame();
    auto &bosses = game.getBosses();
    bosses.clear();

    // place the boss so its grid rect lands fully inside the screen
    const int16_t bx = static_cast<int16_t>(data->hitbox.x + 2);
    const int16_t by = static_cast<int16_t>(data->hitbox.y + 2);
    bosses.emplace_back(bx, by, data);
    CBoss &boss = bosses.back();
    const int num = boss.currentFrame();
    if (num < 0 || num >= 1024)
    {
        bosses.clear();
        return true; // animation beyond the fixture sheet: skip
    }
    const int gridW = data->hitbox.x + 10;
    const int gridH = data->hitbox.y + 10;
    CFrame bmp(gridW * 8, gridH * 8);
    bmp.fill(BLACK);
    rt.drawBossses(bmp, 0, 0, gridW, gridH);
    const CFrame &frame = *(data->sheet == 0 ? rt.sheet0_()->operator[](num)
                                             : rt.sheet1_()->operator[](num));
    const int pxX = (bx - data->hitbox.x) * 8;
    const int pxY = (by - data->hitbox.y) * 8;
    if (px(bmp, pxX, pxY) != frame.getPixelsConst()[0])
        return fail("boss frame pixel");

    // off-screen bosses are skipped
    bosses.clear();
    bosses.emplace_back(2000, 2000, data);
    bmp.fill(BLACK);
    rt.drawBossses(bmp, 0, 0, gridW, gridH);
    if (nonBlack(bmp, 0, 0, bmp.width(), bmp.height()) != 0)
        return fail("off-screen boss drawn");

    // hidden (done) bosses are skipped
    bosses.clear();
    bosses.emplace_back(bx, by, data);
    bosses.back().setState(CBoss::Hidden);
    bmp.fill(BLACK);
    rt.drawBossses(bmp, 0, 0, gridW, gridH);
    if (nonBlack(bmp, 0, 0, bmp.width(), bmp.height()) != 0)
        return fail("hidden boss drawn");
    bosses.clear();
    return true;
}

// --- Phase E: savegame roundtrip ----------------------------------------------
bool test_gamemixin_savegame()
{
    TestRuntime rt;
    rt.ticks() = 123;
    rt.playerFrameOffset_() = 7;
    rt.healthRef_() = 8;
    rt.countdown_() = 9;

    CFileMem mem;
    if (!mem.open("savegame-test", "rw+"))
        return fail("open memory file");
    if (!rt.write(mem, "tester"))
        return fail("write savegame");

    rt.ticks() = 0;
    rt.playerFrameOffset_() = 0;
    rt.healthRef_() = 0;
    rt.countdown_() = 0;
    if (!mem.seek(0))
        return fail("seek");
    std::string name;
    if (!rt.read(mem, name))
        return fail("read savegame");
    if (name != "tester")
        return fail("savegame name");
    if (rt.ticks() != 123 || rt.playerFrameOffset_() != 7 ||
        rt.healthRef_() != 8 || rt.countdown_() != 9)
        return fail("savegame fields");
    return true;
}

bool test_gamemixin()
{
    TestRuntime rt;
    CGame &game = *CGame::getGame();

    // ---- rankUserScore -------------------------------------------------
    constexpr int NB_SCORES = TestRuntime::MAX_SCORES;
    rt.clearScores();
    const int score = game.score();

    // score at/below the worst rank: no placement
    for (int i = 0; i < NB_SCORES; ++i)
        rt.scoreAt(i).score = 1000000 + i;
    if (score >= 1000000 + NB_SCORES - 1)
    {
        LOGE("test_gamemixin: accumulated score %d too high for fixture", score);
        return false;
    }
    if (rt.rankUserScore() != TestRuntime::INVALID)
    {
        LOGE("test_gamemixin: low score was ranked");
        return false;
    }

    // descending table: score[j] = score + 500 - 100*j inserts at j == 6
    for (int j = 0; j < NB_SCORES; ++j)
        rt.scoreAt(j).score = score + 500 - 100 * j;
    const int shifted = score + 500 - 100 * 6;
    if (rt.rankUserScore() != 6)
    {
        LOGE("test_gamemixin: expected rank 6 for score %d", score);
        return false;
    }
    if (rt.scoreAt(6).score != score || rt.scoreAt(6).level != game.level() + 1 ||
        rt.scoreAt(6).name[0] != 0 || rt.scoreAt(7).score != shifted)
    {
        LOGE("test_gamemixin: insertion/shift at rank 6 wrong");
        return false;
    }
    rt.clearScores();

    // ---- getEventText --------------------------------------------------
    struct
    {
        int event;
        const char *line;
        Color color;
    } events[] = {
        {EVENT_SECRET, "SECRET !", CYAN},
        {EVENT_PASSAGE, "PASSAGE", LIGHTGRAY},
        {EVENT_EXTRA_LIFE, "EXTRA LIFE", GREEN},
        {EVENT_GOD_MODE, "GOD MODE!", BLACK},
        {EVENT_RAGE, "RAGE !!!", DARKORANGE},
        {EVENT_FREEZE, "FREEZE TRAP", WHITE},
        {EVENT_TRAP, "TRAP", RED},
        {EVENT_BOAT, "BOAT FOUND", BROWN},
    };
    for (const auto &e : events)
    {
        if (!checkMessage(rt, e.event, e.line, e.color, 100, e.line))
            return false;
    }

    // SUGAR RUSH alternates color every 8 ticks
    rt.ticks() = 0;
    if (!checkMessage(rt, EVENT_SUGAR_RUSH, "SUGAR RUSH", ORANGE, 100, "sugar rush low"))
        return false;
    rt.ticks() = 8;
    if (!checkMessage(rt, EVENT_SUGAR_RUSH, "SUGAR RUSH", LIME, 100, "sugar rush high"))
        return false;

    // YUMMY shows the sugar counter
    game.stats().set(S_SUGAR, 3);
    rt.event() = EVENT_SUGAR;
    if (rt.getEventText(100).lines[0] !=
        "YUMMY 3/" + std::to_string(CGame::MAX_SUGAR_RUSH_LEVEL))
    {
        LOGE("test_gamemixin: YUMMY text wrong");
        return false;
    }
    game.stats().set(S_SUGAR, 0);

    // SHIELD percentage
    game.stats().set(S_SHIELD, 2);
    rt.event() = EVENT_SHIELD;
    if (rt.getEventText(100).lines[0] != "SHIELD 50%")
    {
        LOGE("test_gamemixin: SHIELD text wrong");
        return false;
    }
    game.stats().set(S_SHIELD, 0);

    // EXIT DOOR flashes and is raised one tile
    rt.ticks() = 8;
    rt.event() = EVENT_EXIT_OPENED;
    auto msg = rt.getEventText(100);
    if (msg.lines[0] != "EXIT DOOR IS OPENED" || msg.baseY != 100 - 16)
    {
        LOGE("test_gamemixin: EXIT_OPENED on phase wrong");
        return false;
    }
    rt.ticks() = 0;
    if (!rt.getEventText(100).lines[0].empty())
    {
        LOGE("test_gamemixin: EXIT_OPENED off phase not blank");
        return false;
    }
    rt.ticks() = 0;

    // MSG0..MSGF read their text from the map states
    game.getMap().states().setS(MSG0, "alpha\nbeta");
    rt.event() = MSG0;
    msg = rt.getEventText(100);
    if (msg.lines[0] != "alpha" || msg.lines[1] != "beta" || !msg.lines[2].empty() ||
        msg.color != DARKGRAY || msg.scaleX != 1)
    {
        LOGE("test_gamemixin: MSG0 lines wrong");
        return false;
    }

    // empty MSG state string: no abort, blank lines, raised baseY
    game.getMap().states().setS(MSG0, "");
    msg = rt.getEventText(100);
    if (!msg.lines[0].empty() || !msg.lines[1].empty() || !msg.lines[2].empty() ||
        msg.baseY != 100 - 14)
    {
        LOGE("test_gamemixin: empty MSG0 not blank");
        return false;
    }
    game.getMap().states().setS(MSG0, "alpha\nbeta");

    // unhandled event: empty message
    rt.event() = EVENT_NONE;
    if (!rt.getEventText(100).lines[0].empty())
    {
        LOGE("test_gamemixin: EVENT_NONE produced text");
        return false;
    }

    // ---- manageTimer ---------------------------------------------------
    const int timeBefore = game.timeTaken();
    rt.timer() = 5;
    rt.manageTimer();
    if (rt.timer() != 4 || game.timeTaken() != timeBefore)
    {
        LOGE("test_gamemixin: timer did not just decrement");
        return false;
    }

    CStates &states = game.getMap().states();
    const uint16_t timeoutBefore = states.getU(TIMEOUT);

    // expired timer: timeTaken advances, timer reloads, no timeout set
    states.setU(TIMEOUT, 0);
    rt.timer() = 0;
    rt.manageTimer();
    if (rt.timer() != TestRuntime::tickRate() || game.timeTaken() != timeBefore + 1)
    {
        LOGE("test_gamemixin: expired timer did not reload");
        return false;
    }

    // active timeout counts down each expiry (beep path: m_sound is null)
    states.setU(TIMEOUT, 100);
    rt.timer() = 0;
    rt.manageTimer();
    if (states.getU(TIMEOUT) != 99)
    {
        LOGE("test_gamemixin: TIMEOUT 100 did not decrement");
        return false;
    }
    states.setU(TIMEOUT, 15);
    rt.timer() = 0;
    rt.manageTimer();
    if (states.getU(TIMEOUT) != 14)
    {
        LOGE("test_gamemixin: TIMEOUT 15 did not decrement");
        return false;
    }
    states.setU(TIMEOUT, timeoutBefore);

    // ---- moveCamera ----------------------------------------------------
    const int widthBefore = rt.width_();
    const int heightBefore = rt.height_();
    rt.setWidth(160);
    rt.setHeight(160);
    if (!game.getMap().resize(20, 15, 0, true))
    {
        LOGE("test_gamemixin: map resize failed");
        return false;
    }
    // view 10x10 tiles, deadzones 4; camera coords are half-tiles
    struct
    {
        Pos player;
        int cx;
        int cy;
        int dxCam; // expected camera delta, only one axis moves per call
        int dyCam;
    } cases[] = {
        // up-left: vertical branch takes priority
        {{4, 4}, 10, 10, 0, -1},
        // left edge, vertically centred: horizontal branch
        {{4, 14}, 10, 10, -1, 0},
        // down-right: vertical branch takes priority
        {{16, 16}, 8, 8, 0, +1},
        // right edge, vertically centred: horizontal branch
        {{16, 8}, 8, 8, +1, 0},
        // inside both deadzones: camera stays
        {{10, 10}, 10, 10, 0, 0},
    };
    for (const auto &c : cases)
    {
        game.player().setPos(c.player);
        rt.camX() = c.cx;
        rt.camY() = c.cy;
        rt.moveCamera();
        if (rt.camX() != c.cx + c.dxCam || rt.camY() != c.cy + c.dyCam)
        {
            LOGE("test_gamemixin: moveCamera from player(%d,%d): cam (%d,%d) -> (%d,%d)",
                 c.player.x, c.player.y, c.cx, c.cy, rt.camX(), rt.camY());
            return false;
        }
    }
    rt.setWidth(widthBefore);
    rt.setHeight(heightBefore);
    return true;
}
