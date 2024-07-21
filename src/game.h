/*
    cs3-runtime-sdl
    Copyright (C) 2024  Francois Blanchette

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
#ifndef __GAME_H
#define __GAME_H
#include <stdint.h>
#include <cstdio>
#include "actor.h"
#include "map.h"

class CMapArch;
class CSndArray;
class IFile;
class CSnd;
class ISound;

class CGame
{
public:
    CGame();
    ~CGame();

    bool init();
    bool loadLevel(bool restart);
    bool move(int dir);
    void manageMonsters(int ticks);
    void managePlayer(uint8_t *joystate);
    static Pos translate(const Pos &p, int aim);
    void consume();
    static bool hasKey(uint8_t c);
    void addKey(uint8_t c);
    bool goalCount() const;
    static CMap &getMap();
    void nextLevel();
    void restartLevel();
    void restartGame();
    void setMode(int mode);
    int mode() const;
    bool isPlayerDead();
    void killPlayer();
    bool isGameOver();
    CActor &player();
    int score();
    int lives();
    int diamonds();
    int health();
    void setMapArch(CMapArch *arch);
    void setLevel(int levelId);
    int level();
    int godModeTimer();
    int playerSpeed();
    static uint8_t *keys();
    void getMonsters(CActor *&monsters, int &count);
    CActor &getMonster(int i);
    bool readSndArch(IFile &file);
    void playSound(int id);
    void playTileSound(int tileID);

    enum GameMode
    {
        MODE_INTRO,
        MODE_LEVEL,
        MODE_RESTART,
        MODE_GAMEOVER,
        MODE_CLICKSTART,
        MODE_HISCORES,
        MODE_IDLE,
    };

protected:
    enum
    {
        DEFAULT_MAX_MONSTERS = 128,
        GROWBY_MONSTERS = 64,
        MAX_HEALTH = 255,
        DEFAULT_HEALTH = 64,
        DEFAULT_LIVES = 5,
        LEVEL_BONUS = 500,
        SCORE_LIFE = 5000,
        MAX_LIVES = 99,
        GODMODE_TIMER = 100,
        EXTRASPEED_TIMER = 200,
        DEFAULT_PLAYER_SPEED = 3,
        FAST_PLAYER_SPEED = 2,
        INVALID = -1,
        VERSION = (0x0200 << 16) + 0x0000,
        MAX_KEYS = 6,
    };

    int m_lives = 0;
    int m_health = 0;
    int m_level = 0;
    int m_score = 0;
    int m_nextLife = SCORE_LIFE;
    int m_diamonds = 0;
    int32_t m_godModeTimer = 0;
    int32_t m_extraSpeedTimer = 0;
    static uint8_t m_keys[MAX_KEYS];
    int m_mode;
    // monsters
    CActor *m_monsters;
    int m_monsterCount;
    int m_monsterMax;
    CActor m_player;
    CMapArch *m_mapArch = nullptr;
    ISound *m_sound = nullptr;
    uint8_t m_soundMap[256];

    int clearAttr(uint8_t attr);
    bool findMonsters();
    int addMonster(const CActor actor);
    int findMonsterAt(int x, int y);
    void addHealth(int hp);
    void addPoints(int points);
    void addLife();
    void vDebug(const char *format, ...);
    bool read(FILE *sfile);
    bool write(FILE *tfile);

    friend class CGameMixin;
};
#endif
