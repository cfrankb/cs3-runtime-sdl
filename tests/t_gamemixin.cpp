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
#include "t_gamemixin.h"
#include "../src/runtime.h"
#include "../src/game.h"
#include "../src/gamestats.h"
#include "../src/states.h"
#include "../src/statedata.h"
#include "../src/events.h"
#include "../src/actor.h"
#include "../src/logger.h"

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

    int &event() { return m_currentEvent; }
    int &timer() { return m_timer; }
    uint32_t &ticks() { return m_ticks; }
    int &camX() { return m_cx; }
    int &camY() { return m_cy; }
    hiscore_t &scoreAt(int i) { return m_hiscores[i]; }
    int width_() { return getWidth(); }
    int height_() { return getHeight(); }
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
