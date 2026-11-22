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

// Engine (engine.cpp) is an abstract base with zero SDL references; the
// TestEngine stub below records the eight pure-virtual draw hooks so the
// real dispatch/layout logic in engine.cpp runs headless.

#include <cstring>
#include "../../src/engine.h"
#include "../../src/runtime.h"
#include "../../src/game.h"
#include "../../src/map.h"
#include "../../src/layer.h"
#include "../../src/actor.h"
#include "../../src/gamesfx.h"
#include "../../src/gamestats.h"
#include "../../src/statedata.h"
#include "../../src/states.h"
#include "../../src/events.h"
#include "../../src/tilesdata.h"
#include "../../src/chars.h"
#include "../../src/menu.h"
#include "../../src/menumanager.h"
#include "../../src/animator.h"
#include "../../src/gameui.h"
#include "../../src/theme.h"
#include "../../src/summary.h"
#include "../../src/logger.h"

namespace
{
class TestEngine : public Engine
{
public:
    struct FontCall
    {
        int x;
        int y;
        std::string text;
        Color color;
        int scaleX;
        int scaleY;
    };
    struct RectCall
    {
        rect_t rect;
        Color color;
        bool fill;
    };
    std::vector<FontCall> fonts;
    std::vector<RectCall> rects;
    int screens = 0;
    int colorMaps = 0;
    int splashes = 0;
    int titlePixCalls = 0;
    int menuDraws = 0;
    int menuX = 0;
    int menuY = 0;
    CMenu *lastMenu = nullptr;
    int fazes = 0;
    int fazeStr = -1;

    void drawScreen() override { ++screens; }
    void updateColorMaps(const ColorMaps &) override { ++colorMaps; }
    void drawMenu(CMenu &menu, const int x, const int y) override
    {
        ++menuDraws;
        lastMenu = &menu;
        menuX = x;
        menuY = y;
    }
    void drawRect(const rect_t &r, const Color &c, const bool fill) override
    {
        rects.push_back({r, c, fill});
    }
    void drawFont(const int x, const int y, const char *text,
                  const Color color = WHITE, const int scaleX = 1,
                  const int scaleY = 1) override
    {
        fonts.push_back({x, y, text, color, scaleX, scaleY});
    }
    int drawTitlePix(int) override
    {
        ++titlePixCalls;
        return 40;
    }
    void drawSplash() override { ++splashes; }
    void fazeScreen(int str = 0xe0) override
    {
        ++fazes;
        fazeStr = str;
    }

    // protected seam exposure (t_gamemixin pattern)
    using Engine::cameraContext_t;
    using Engine::gatherSprites;
    using Engine::getEventText;
    using Engine::message_t;
    using Engine::resizeGameMenu;
    using Engine::sprite_t;

    void attach(CGame *game, MenuManager *menus, CAnimator *animator,
                CGameMixin *mixin)
    {
        m_game = game;
        m_menus = menus;
        m_animator = animator;
        m_gameMixin = mixin;
    }
    Summary &summary() { return m_summary; }
    void clearCalls()
    {
        fonts.clear();
        rects.clear();
        menuDraws = 0;
        fazes = 0;
        titlePixCalls = 0;
    }
    bool expectFont(size_t i, const std::string &text, Color color,
                    const char *what)
    {
        if (i >= fonts.size() || fonts[i].text != text || fonts[i].color != color)
        {
            LOGE("engine: %s font[%zu] '%s' color %d; expecting '%s' color %d",
                 what, i, i < fonts.size() ? fonts[i].text.c_str() : "<none>",
                 i < fonts.size() ? (int)fonts[i].color : -1, text.c_str(),
                 (int)color);
            return false;
        }
        return true;
    }
};

// CRuntime is default-constructible headless (see t_gamemixin); it doubles as
// the CGameMixin instance Engine needs and as the drift-guard twin of
// Engine::getEventText.
class TestMixin : public CRuntime
{
public:
    using CRuntime::clearScores;
    using CRuntime::getEventText;
    using CRuntime::INVALID;

    int &event() { return m_currentEvent; }
    uint32_t &ticks() { return m_ticks; }
    int &scoreRank_() { return m_scoreRank; }
    bool &recordScore_() { return m_recordScore; }
};

EngineState eventState(const int event, const int countdown = 0)
{
    EngineState s{};
    s.m_currentEvent = event;
    s.m_countdown = countdown;
    return s;
}
} // namespace

bool test_engine_gather_sprites()
{
    CGame *game = CGame::getGame();
    CMap &map = game->getMap();
    map.clear();
    map.resize(10, 10, TILES_BLANK, true);
    map.getMainLayer()->set(2, 2, TILES_ZOMBIE);
    map.getMainLayer()->set(5, 5, TILES_ZOMBIE);
    map.getMainLayer()->set(1, 1, TILES_BLANK);

    auto &monsters = game->getMonsters();
    monsters.clear();
    monsters.emplace_back(2, 2, TILES_ZOMBIE); // in viewport, special tile
    monsters.emplace_back(5, 5, TILES_ZOMBIE); // out of viewport
    monsters.emplace_back(1, 1, TILES_BLANK);  // in viewport, plain tile
    game->rebuildMonsterGrid();

    auto &sfx = game->getSfx();
    sfx.clear();
    sfx.push_back({3, 1, static_cast<Sfx>(MIN_SFX + 1), 10}); // inside
    sfx.push_back({9, 9, static_cast<Sfx>(MIN_SFX + 2), 10}); // outside

    TestEngine engine;
    engine.attach(game, nullptr, game->getAnimator(), nullptr);
    engine.resize(64, 48); // 4 cols x 3 rows visible

    std::vector<TestEngine::sprite_t> sprites;
    const TestEngine::cameraContext_t ctx{0, 0, 0, 0};
    engine.gatherSprites(sprites, ctx);

    monsters.clear();
    sfx.clear();
    game->rebuildMonsterGrid();

    if (sprites.size() != 2)
    {
        LOGE("gatherSprites kept %zu sprites; expecting 2", sprites.size());
        return false;
    }
    if (sprites[0].tileID != TILES_ZOMBIE || sprites[0].x != 2 || sprites[0].y != 2)
    {
        LOGE("gatherSprites monster sprite (%d,%d) tile 0x%x", sprites[0].x,
             sprites[0].y, sprites[0].tileID);
        return false;
    }
    if (sprites[1].tileID != MIN_SFX + 1 || sprites[1].x != 3 || sprites[1].y != 1)
    {
        LOGE("gatherSprites sfx sprite (%d,%d) tile 0x%x", sprites[1].x,
             sprites[1].y, sprites[1].tileID);
        return false;
    }

    // empty viewport keeps nothing
    std::vector<TestEngine::sprite_t> none;
    const TestEngine::cameraContext_t farCtx{100, 0, 100, 0};
    engine.gatherSprites(none, farCtx);
    if (!none.empty())
    {
        LOGE("gatherSprites kept %zu sprites off-screen", none.size());
        return false;
    }
    return true;
}

bool test_engine_event_text()
{
    CGame *game = CGame::getGame();
    TestEngine engine;
    engine.attach(game, nullptr, nullptr, nullptr);

    struct
    {
        int event;
        const char *line;
        Color color;
    } cases[] = {
        {EVENT_SECRET, "SECRET !", CYAN},
        {EVENT_PASSAGE, "PASSAGE", LIGHTGRAY},
        {EVENT_EXTRA_LIFE, "EXTRA LIFE", GREEN},
        {EVENT_GOD_MODE, "GOD MODE!", BLACK},
        {EVENT_RAGE, "RAGE !!!", DARKORANGE},
        {EVENT_FREEZE, "FREEZE TRAP", WHITE},
        {EVENT_TRAP, "TRAP", RED},
        {EVENT_BOAT, "BOAT FOUND", BROWN},
    };
    for (const auto &c : cases)
    {
        engine.updateEngineState(eventState(c.event));
        engine.setTicks(0);
        const auto msg = engine.getEventText(100);
        if (msg.lines[0] != c.line || msg.color != c.color || msg.baseY != 100 ||
            msg.scaleX != 2 || msg.scaleY != 1)
        {
            LOGE("event 0x%x: '%s' color %d baseY %d scales %d/%d", c.event,
                 msg.lines[0].c_str(), (int)msg.color, msg.baseY, msg.scaleX,
                 msg.scaleY);
            return false;
        }
    }

    // SUGAR RUSH colour blinks with the tick bit
    engine.updateEngineState(eventState(EVENT_SUGAR_RUSH));
    engine.setTicks(8);
    if (engine.getEventText(100).color != LIME)
    {
        LOGE("sugar rush at tick 8 not LIME");
        return false;
    }
    engine.setTicks(0);
    {
        const auto msg = engine.getEventText(100);
        if (msg.color != ORANGE || msg.scaleX != 2 || msg.scaleY != 2)
        {
            LOGE("sugar rush at tick 0 not ORANGE 2/2");
            return false;
        }
    }

    // SUGAR mirrors game->sugar()
    engine.updateEngineState(eventState(EVENT_SUGAR));
    {
        char want[40];
        snprintf(want, sizeof(want), "YUMMY %d/%d", game->sugar(),
                 CGame::MAX_SUGAR_RUSH_LEVEL);
        const auto msg = engine.getEventText(100);
        if (msg.lines[0] != want || msg.color != PURPLE)
        {
            LOGE("sugar text '%s' expecting '%s'", msg.lines[0].c_str(), want);
            return false;
        }
    }

    // SHIELD mirrors the stat
    engine.updateEngineState(eventState(EVENT_SHIELD));
    {
        char want[40];
        snprintf(want, sizeof(want), "SHIELD %d%%",
                 25 * game->stats().get(S_SHIELD));
        const auto msg = engine.getEventText(100);
        if (msg.lines[0] != want || msg.color != LIME)
        {
            LOGE("shield text '%s' expecting '%s'", msg.lines[0].c_str(), want);
            return false;
        }
    }

    // EXIT_OPENED blinks and lifts the text by one tile
    engine.updateEngineState(eventState(EVENT_EXIT_OPENED));
    engine.setTicks(8);
    {
        const auto msg = engine.getEventText(100);
        if (msg.lines[0] != "EXIT DOOR IS OPENED" || msg.baseY != 84 ||
            msg.color != SEAGREEN)
        {
            LOGE("exit-opened '%s' baseY %d", msg.lines[0].c_str(), msg.baseY);
            return false;
        }
    }
    engine.setTicks(0);
    if (!engine.getEventText(100).lines[0].empty())
    {
        LOGE("exit-opened not blanked at tick 0");
        return false;
    }

    // MSG0 splits map state text into up to three lines
    game->getMap().states().setS(MSG0, "alpha\nbeta");
    engine.updateEngineState(eventState(MSG0));
    {
        const auto msg = engine.getEventText(100);
        if (msg.lines[0] != "alpha" || msg.lines[1] != "beta" ||
            !msg.lines[2].empty() || msg.color != DARKGRAY || msg.baseY != 84)
        {
            LOGE("msg0 '%s' '%s' baseY %d", msg.lines[0].c_str(),
                 msg.lines[1].c_str(), msg.baseY);
            return false;
        }
    }

    // unknown event: warned, empty message
    engine.updateEngineState(eventState(EVENT_NONE));
    if (!engine.getEventText(100).lines[0].empty())
    {
        LOGE("EVENT_NONE produced text");
        return false;
    }

    // drift guard: Engine::getEventText must agree with CGameMixin's twin
    TestMixin rt;
    const int driftEvents[] = {EVENT_SECRET, EVENT_PASSAGE, EVENT_EXTRA_LIFE,
                               EVENT_SUGAR_RUSH, EVENT_GOD_MODE, EVENT_SUGAR,
                               MSG0, EVENT_RAGE, EVENT_FREEZE, EVENT_TRAP,
                               EVENT_EXIT_OPENED, EVENT_SHIELD, EVENT_BOAT};
    for (const int ev : driftEvents)
    {
        for (const uint32_t tick : {0u, 8u})
        {
            engine.updateEngineState(eventState(ev));
            engine.setTicks(tick);
            rt.event() = ev;
            rt.ticks() = tick;
            const auto mine = engine.getEventText(64);
            const auto theirs = rt.getEventText(64);
            if (mine.lines[0] != theirs.lines[0] ||
                mine.lines[1] != theirs.lines[1] ||
                mine.lines[2] != theirs.lines[2] || mine.color != theirs.color ||
                mine.scaleX != theirs.scaleX || mine.scaleY != theirs.scaleY ||
                mine.baseY != theirs.baseY)
            {
                LOGE("getEventText drift at event 0x%x tick %u: engine '%s' "
                     "mixin '%s'",
                     ev, tick, mine.lines[0].c_str(), theirs.lines[0].c_str());
                return false;
            }
        }
    }
    return true;
}

bool test_engine_level_summary()
{
    CGame *game = CGame::getGame();
    MenuManager menus;
    TestMixin mixin;
    TestEngine engine;
    engine.attach(game, &menus, nullptr, &mixin);

    // initLevelSummary over an empty map: no counters -> 100 %
    const int savedLevel = game->level();
    const int savedLives = game->lives();
    game->setLevel(0);
    engine.updateEngineState(eventState(EVENT_NONE, 1));
    engine.resize(400, 300);
    engine.setTicks(0);
    engine.initLevelSummary();
    auto &sum = engine.summary();
    // pin the percentage formula against the game's own reports
    {
        const MapReport r0 = game->originalMapReport();
        const MapReport r1 = game->currentMapReport();
        const int wantFruits = r0.fruits ? 100 * (r0.fruits - r1.fruits) / r0.fruits : 100;
        const int wantBonuses = r0.bonuses ? 100 * (r0.bonuses - r1.bonuses) / r0.bonuses : 100;
        const int wantSecrets = r0.secrets ? 100 * (r0.secrets - r1.secrets) / r0.secrets : 100;
        if (sum.ppFruits != wantFruits || sum.ppBonuses != wantBonuses ||
            sum.ppSecrets != wantSecrets)
        {
            LOGE("summary %d/%d/%d; formula %d/%d/%d", sum.ppFruits,
                 sum.ppBonuses, sum.ppSecrets, wantFruits, wantBonuses,
                 wantSecrets);
            return false;
        }
    }
    if (sum.timeTaken != game->timeTaken())
    {
        LOGE("summary timeTaken %d; game %d", sum.timeTaken, game->timeTaken());
        return false;
    }

    // seven lines; wide layout uses 2x header
    engine.clearCalls();
    engine.drawLevelSummary();
    char want[128];
    snprintf(want, sizeof(want), "LEVEL %.2d COMPLETED", game->level() + 1);
    if (engine.fonts.size() != 7 || !engine.expectFont(0, want, YELLOW, "summary") ||
        engine.fonts[0].scaleX != 2)
    {
        LOGE("wide summary drew %zu lines", engine.fonts.size());
        return false;
    }
    // narrow layout scales the header 1x2
    engine.resize(200, 300);
    engine.clearCalls();
    engine.drawLevelSummary();
    if (engine.fonts.empty() || engine.fonts[0].scaleX != 1 ||
        engine.fonts[0].scaleY != 2)
    {
        LOGE("narrow summary header scale");
        return false;
    }
    // countdown 0 + blinking tick reveals the continue prompt
    engine.updateEngineState(eventState(EVENT_NONE, 0));
    engine.setTicks(8);
    engine.clearCalls();
    engine.drawLevelSummary();
    if (engine.fonts.size() != 7 ||
        engine.fonts[6].text != "PRESS SPACE TO CONTINUE")
    {
        LOGE("continue prompt missing (last='%s')", engine.fonts[6].text.c_str());
        return false;
    }

    // ---- drawScores -----------------------------------------------------
    mixin.clearScores();
    auto &hi = mixin.getHiScoreAtIndex(3);
    hi.score = 1234;
    hi.level = 2;
    memset(hi.name, 0, sizeof(hi.name));
    strcpy(hi.name, "TESTER");
    mixin.scoreRank_() = 3;
    mixin.recordScore_() = true;
    engine.resize(320, 240); // 4:3 -> header 2x2
    engine.setTicks(8);      // caret bit set (8 & CARET_SPEED)
    engine.clearCalls();
    engine.drawScores();
    if (engine.fonts.size() != 21) // header, rule, 18 scores, prompt
    {
        LOGE("scores drew %zu lines; expecting 21", engine.fonts.size());
        return false;
    }
    if (!engine.expectFont(0, "HALL OF HEROES", WHITE, "scores") ||
        engine.fonts[0].scaleX != 2 || engine.fonts[0].scaleY != 2)
    {
        return false;
    }
    const char caret = static_cast<char>(CHARS_CARET - CHARS_CUSTOM +
                                         CHARS_CUSTOM_BASE + ' ');
    snprintf(want, sizeof(want), " %.8d %.2d %s%c", 1234, 2, "TESTER", caret);
    if (!engine.expectFont(2 + 3, want, YELLOW, "ranked row"))
    {
        return false;
    }
    if (engine.fonts[2 + 4].color != DARKGRAY || engine.fonts[2 + 6].color != LIGHTGRAY)
    {
        LOGE("unranked row colors %d/%d; expecting %d/%d",
             (int)engine.fonts[2 + 4].color, (int)engine.fonts[2 + 6].color,
             (int)DARKGRAY, (int)LIGHTGRAY);
        return false;
    }
    if (engine.fonts[20].text != "PLEASE TYPE YOUR NAME AND PRESS ENTER.")
    {
        LOGE("record-score prompt missing ('%s')", engine.fonts[20].text.c_str());
        return false;
    }

    // ranked but not recording: cyan row, no prompt
    mixin.recordScore_() = false;
    engine.clearCalls();
    engine.drawScores();
    if (engine.fonts.size() != 20 ||
        engine.fonts[2 + 3].color != CYAN)
    {
        LOGE("non-recording scores: %zu lines, row color %d",
             engine.fonts.size(), (int)engine.fonts[2 + 3].color);
        return false;
    }

    // no rank at all: apology line, no cyan row
    mixin.scoreRank_() = TestMixin::INVALID;
    engine.clearCalls();
    engine.drawScores();
    if (engine.fonts.size() != 21 ||
        engine.fonts[20].text != " SORRY, YOU DIDN'T QUALIFY.")
    {
        LOGE("unranked footer missing ('%s')", engine.fonts[20].text.c_str());
        return false;
    }

    // non-4:3 halves the header scale
    engine.resize(300, 300);
    engine.clearCalls();
    engine.drawScores();
    if (engine.fonts.empty() || engine.fonts[0].scaleX != 1 ||
        engine.fonts[0].scaleY != 1)
    {
        LOGE("non-4:3 header scale %d/%d", engine.fonts[0].scaleX,
             engine.fonts[0].scaleY);
        return false;
    }

    // ---- drawHelpScreen -------------------------------------------------
    auto &help = mixin.helptext();
    help = {"~HEAD", "!MID", "plain"};
    engine.resize(200, 100);
    engine.clearCalls();
    engine.drawHelpScreen();
    help.clear();
    if (engine.fonts.size() != 3 || !engine.expectFont(0, "HEAD", YELLOW, "help") ||
        engine.fonts[0].x != 0 ||
        !engine.expectFont(1, "MID", WHITE, "help") ||
        engine.fonts[1].x != (200 - 3 * 8) / 2 ||
        !engine.expectFont(2, "plain", WHITE, "help") || engine.fonts[2].x != 0)
    {
        return false;
    }

    // ---- menu dispatch screens ------------------------------------------
    engine.clearCalls();
    engine.drawOptions();
    if (engine.menuDraws != 1 || engine.lastMenu != menus.get(MENUID_OPTIONMENU) ||
        engine.menuX != 48 || engine.menuY != 32)
    {
        LOGE("drawOptions menu (%d,%d)", engine.menuX, engine.menuY);
        return false;
    }
    engine.clearCalls();
    engine.drawUserMenu();
    if (engine.menuDraws != 1 || engine.lastMenu != menus.get(MENUID_USERS) ||
        engine.menuX != 48 || !engine.expectFont(0, "SELECT DIAMOND HUNTER", GRAY, "usermenu"))
    {
        LOGE("drawUserMenu mismatch");
        return false;
    }
    engine.clearCalls();
    engine.drawSkillMenu();
    if (engine.menuDraws != 1 || engine.lastMenu != menus.get(MENUID_SKILLS) ||
        engine.menuX != -1 ||
        !engine.expectFont(0, "GAME DIFFICULTY", GRAY, "skillmenu"))
    {
        LOGE("drawSkillMenu mismatch");
        return false;
    }

    game->setLevel(savedLevel);
    game->setLives(savedLives);
    return true;
}

bool test_engine_resize_game_menu()
{
    CGame *game = CGame::getGame();
    MenuManager menus;
    TestEngine engine;
    engine.attach(game, &menus, nullptr, nullptr);
    CMenu *menu = menus.get(MENUID_GAMEMENU);

    struct
    {
        int width;
        int scaleX;
        int scaleY;
    } cases[] = {
        {600, 4, 3}, // 1200 > 1024
        {450, 3, 3}, // 900 > 800
        {320, 2, 2}, // 640 >= MIN_WIDTH_FULL * 2
        {300, 1, 2}, // small fallback
    };
    for (const auto &c : cases)
    {
        engine.resize(c.width, 300);
        engine.resizeGameMenu();
        if (menu->scaleX() != c.scaleX || menu->scaleY() != c.scaleY)
        {
            LOGE("resizeGameMenu width %d -> scale %d/%d; expecting %d/%d",
                 c.width, menu->scaleX(), menu->scaleY(), c.scaleX, c.scaleY);
            return false;
        }
    }
    return true;
}

bool test_engine_keyboard()
{
    CGame *game = CGame::getGame();
    TestEngine engine;
    engine.attach(game, nullptr, nullptr, nullptr);

    CGameUI *ui = engine.virtualKeyboard();
    const auto &buttons = ui->buttons();
    if (buttons.size() != 26 + 10 + 10 + 3)
    {
        LOGE("keyboard has %zu buttons; expecting 49", buttons.size());
        return false;
    }
    if (buttons[0].id != 'A' || buttons[26].id != '0' || buttons[45].id != '!' ||
        buttons[46].id != ' ' || buttons[47].id != 8 || buttons[48].id != '\n')
    {
        LOGE("keyboard layout ids: 0='%c' 26='%c' 46=%d 47=%d 48=%d",
             buttons[0].id, buttons[26].id, buttons[46].id, buttons[47].id,
             buttons[48].id);
        return false;
    }

    engine.resize(300, 200);
    engine.setTicks(16); // caret bit set ((16 >> 4) & 1)
    engine.clearCalls();
    std::string buffer = "AB";
    engine.drawVirtualKeyboard("TITLE", buffer);
    if (engine.rects.size() != buttons.size() * 2)
    {
        LOGE("drawUI rects %zu; expecting %zu", engine.rects.size(),
             buttons.size() * 2);
        return false;
    }
    const int baseX = 300 - ui->width() - ui->margin();
    if (engine.rects[0].rect.x != baseX + buttons[0].x ||
        engine.rects[0].fill != true || engine.rects[1].color != RED)
    {
        LOGE("drawUI first rect mismatch");
        return false;
    }
    const size_t n = engine.fonts.size();
    if (n != buttons.size() + 2)
    {
        LOGE("keyboard fonts %zu; expecting %zu", n, buttons.size() + 2);
        return false;
    }
    if (!engine.expectFont(n - 2, "TITLE", LIGHTGRAY, "kb title") ||
        engine.fonts[n - 2].x != (300 - 5 * 8 * 2) / 2 ||
        engine.fonts[n - 2].scaleX != 2)
    {
        return false;
    }
    const std::string typed = std::string("AB") + static_cast<char>(CHARS_CARET);
    if (!engine.expectFont(n - 1, typed, YELLOW, "kb buffer"))
    {
        return false;
    }

    // caret off on the other half of the blink
    engine.setTicks(0);
    engine.clearCalls();
    engine.drawVirtualKeyboard("TITLE", buffer);
    if (engine.fonts.back().text != "AB ")
    {
        LOGE("kb buffer without caret is '%s'", engine.fonts.back().text.c_str());
        return false;
    }
    return true;
}

bool test_engine_screens()
{
    CGame *game = CGame::getGame();
    MenuManager menus;
    TestEngine engine;
    engine.attach(game, &menus, nullptr, nullptr);
    engine.resize(320, 240);

    // stub hooks are just counted
    engine.drawScreen();
    engine.fazeScreen();
    engine.fazeScreen(128);
    if (engine.screens != 1 || engine.fazes != 2 || engine.fazeStr != 128)
    {
        LOGE("stub counters screens %d fazes %d str %d", engine.screens,
             engine.fazes, engine.fazeStr);
        return false;
    }
    ColorMaps colormaps;
    engine.updateColorMaps(colormaps);
    if (engine.colorMaps != 1)
    {
        LOGE("updateColorMaps not recorded");
        return false;
    }

    engine.clearCalls();
    engine.drawPreScreen();
    if (!engine.expectFont(0, "CLICK TO START", WHITE, "prescreen") ||
        engine.fonts[0].x != (320 - 14 * 8) / 2)
    {
        return false;
    }

    engine.clearCalls();
    engine.drawTest();
    if (!engine.expectFont(0, "COMING SOON 2026", YELLOW, "drawTest") ||
        engine.fonts[0].scaleX != 2)
    {
        return false;
    }

    // level intro per game mode
    const auto savedMode = game->mode();
    const int savedLevel = game->level();
    const int savedLives = game->lives();
    char want[64];

    game->setLevel(0);
    game->setMode(CGame::MODE_LEVEL_INTRO);
    engine.clearCalls();
    engine.drawLevelIntro();
    snprintf(want, sizeof(want), "LEVEL 01");
    if (!engine.expectFont(0, want, YELLOW, "intro") || engine.fonts.size() < 2)
    {
        game->setMode(savedMode);
        return false;
    }

    game->setMode(CGame::MODE_RESTART);
    game->setLives(3);
    engine.clearCalls();
    engine.drawLevelIntro();
    if (!engine.expectFont(0, "LIVES LEFT 03", YELLOW, "restart"))
    {
        game->setMode(savedMode);
        return false;
    }
    game->setLives(1);
    engine.clearCalls();
    engine.drawLevelIntro();
    if (!engine.expectFont(0, "LAST LIFE !", YELLOW, "last life"))
    {
        game->setMode(savedMode);
        return false;
    }

    game->setMode(CGame::MODE_GAMEOVER);
    engine.clearCalls();
    engine.drawLevelIntro();
    if (!engine.expectFont(0, "GAME OVER", YELLOW, "gameover") ||
        engine.fonts.size() != 1) // game-over skips title and hint lines
    {
        game->setMode(savedMode);
        return false;
    }

    game->setMode(CGame::MODE_TIMEOUT);
    engine.clearCalls();
    engine.drawLevelIntro();
    if (!engine.expectFont(0, "OUT OF TIME", YELLOW, "timeout"))
    {
        game->setMode(savedMode);
        return false;
    }

    game->setMode(CGame::MODE_CHUTE);
    engine.clearCalls();
    engine.drawLevelIntro();
    if (!engine.expectFont(0, "DROP TO LEVEL 01", YELLOW, "chute"))
    {
        game->setMode(savedMode);
        return false;
    }

    // narrow screens drop the hint line but keep the title
    engine.resize(200, 240);
    game->setMode(CGame::MODE_RESTART);
    engine.clearCalls();
    engine.drawLevelIntro();
    if (engine.fonts.size() != 1)
    {
        LOGE("narrow restart drew %zu lines; expecting 1", engine.fonts.size());
        game->setMode(savedMode);
        return false;
    }
    game->setMode(savedMode);
    game->setLevel(savedLevel);
    game->setLives(savedLives);

    // title screen: main menu active gates faze + menu draw
    CMenu *mainMenu = menus.get(MENUID_MAINMENU);
    menus.setActive(MENUID_MAINMENU, true);
    engine.resize(600, 400); // width * 2 > 800 shifts the menu down
    engine.clearCalls();
    engine.drawTitleScreen("SCROLL");
    if (engine.titlePixCalls != 1 || engine.fazes != 1 || engine.fazeStr != 128 ||
        engine.menuDraws != 1 || engine.lastMenu != mainMenu ||
        engine.menuY != 124 || engine.fonts.size() != 2)
    {
        LOGE("active title screen: fazes %d menu %d at %d fonts %zu",
             engine.fazes, engine.menuDraws, engine.menuY, engine.fonts.size());
        return false;
    }
    if (!engine.expectFont(0, "SCROLL", BLACK, "scroll shadow") ||
        engine.fonts[1].color != MENU_COLOR_ITEM_SELECTED ||
        engine.fonts[0].y != engine.fonts[1].y + 2)
    {
        return false;
    }

    menus.setActive(MENUID_MAINMENU, false);
    engine.clearCalls();
    engine.drawTitleScreen("SCROLL");
    if (engine.fazes != 0 || engine.menuDraws != 0 || engine.fonts.size() != 2)
    {
        LOGE("inactive title screen still drew menu");
        return false;
    }
    return true;
}
