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

#include "../../src/logger.h"
#include "../../src/parseargs.h"
#include "../../src/skills.h"
#include "t_parseargs.h"

static bool check(const bool cond, const char *what)
{
    if (!cond)
        LOGE("parseArgs: %s", what);
    return cond;
}

bool test_parseargs()
{
    params_t p;
    bool appExit = false;

    if (!parseArgs({"--hard"}, p, appExit) || p.skill != SKILL_HARD || appExit)
        return check(false, "--hard");
    if (!parseArgs({"--easy"}, p, appExit) || p.skill != SKILL_EASY)
        return check(false, "--easy");

    if (!parseArgs({"-p", "/data", "-m", "/m.mapz", "-w", "/ws"}, p, appExit) ||
        p.prefix != "/data" || p.mapArch != "/m.mapz" || p.workspace != "/ws")
        return check(false, "-p/-m/-w values");

    if (!parseArgs({"-f", "-v", "-q", "-s"}, p, appExit) ||
        !p.fullscreen || !p.verbose || !p.muteMusic || !p.strip_private)
        return check(false, "combined switches");

    if (!parseArgs({"--window", "1920x1080"}, p, appExit) ||
        p.width != 960 || p.height != 540)
        return check(false, "--window halves dimensions");

    if (parseArgs({"--window", "100x100"}, p, appExit))
        return check(false, "--window below 240 should fail");

    if (!parseArgs({"--short"}, p, appExit) || p.width != 240 || p.height != 320)
        return check(false, "--short resolution");

    if (!parseArgs({"3"}, p, appExit) || p.level != 3)
        return check(false, "positional level");

    // error paths
    appExit = false;
    if (parseArgs({"--bogus"}, p, appExit))
        return check(false, "unknown long option should fail");
    if (parseArgs({"-p"}, p, appExit))
        return check(false, "missing -p value should fail");
    if (parseArgs({"-z"}, p, appExit))
        return check(false, "unknown switch should fail");

    // help exits without error
    appExit = false;
    if (!parseArgs({"--help"}, p, appExit) || !appExit)
        return check(false, "--help should set appExit");
    appExit = false;
    if (!parseArgs({"-h"}, p, appExit) || !appExit)
        return check(false, "-h should set appExit");

    return true;
}
