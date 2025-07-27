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
#pragma once
#include <vector>
#include <stdint.h>
#include <cstdio>
#include "actor.h"
#include "map.h"

class CMapArch;
class ISound;

class CGame
{
public:
    CGame();
    ~CGame();

    bool init();
    bool loadLevel(const bool restart);
    bool move(const int dir);
    void manageMonsters(const int ticks);
    uint8_t managePlayer(const uint8_t *joystate);
    static Pos translate(const Pos &p, const int aim);
    void consume();
    static bool hasKey(const uint8_t c);
    void addKey(const uint8_t c);
    bool goalCount() const;
    static CMap &getMap();
    void nextLevel();
    void restartLevel();
    void restartGame();
    void setMode(int mode);
    int mode() const;
    bool isPlayerDead();
    void killPlayer();
    bool isGameOver() const;
    CActor &player();
    const CActor &playerConst() const;
    int score() const;
    int lives() const;
    int diamonds() const;
    int health() const;
    void setMapArch(CMapArch *arch);
    void setLevel(const int levelId);
    int level() const;
    int godModeTimer() const;
    int playerSpeed() const;
    static uint8_t *keys();
    void getMonsters(CActor *&monsters, int &count);
    CActor &getMonster(int i);
    void playSound(const int id) const;
    void playTileSound(const int tileID) const;
    void setLives(const int lives);
    void attach(ISound *s);
    void setSkill(const uint8_t v);
    uint8_t skill() const;
    int size() const;
    void resetStats();
    void parseHints(const char *data);

    enum GameMode
    {
        MODE_LEVEL_INTRO,
        MODE_LEVEL,
        MODE_RESTART,
        MODE_GAMEOVER,
        MODE_CLICKSTART,
        MODE_HISCORES,
        MODE_IDLE,
        MODE_HELP,
        MODE_TITLE
    };

private:
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
        SECRET_TIMER = 12,
        DEFAULT_PLAYER_SPEED = 3,
        FAST_PLAYER_SPEED = 2,
        INVALID = -1,
        VERSION = (0x0200 << 16) + 0x0003,
        MAX_KEYS = 6,
        SOUND_NONE = 0x00,
        SOUND_GRUUP,
        SOUND_KEY,
        SOUND_0009,
        SOUND_COIN1,
        SOUND_OUCHFAST,
        SOUND_POUF,
        SOUND_POW,
    };

    int m_lives = 0;
    int m_health = 0;
    int m_level = 0;
    int m_score = 0;
    int m_nextLife = SCORE_LIFE;
    int m_diamonds = 0;
    int32_t m_godModeTimer = 0;
    int32_t m_extraSpeedTimer = 0;
    int32_t m_secretTimer = 0;
    static uint8_t m_keys[MAX_KEYS];
    int m_mode;
    int m_introHint = 0;

    // monsters
    CActor *m_monsters;
    int m_monsterCount;
    int m_monsterMax;
    CActor m_player;
    CMapArch *m_mapArch = nullptr;
    ISound *m_sound = nullptr;
    uint8_t m_skill;
    std::vector<std::string> m_hints;

    int clearAttr(const uint8_t attr);
    bool findMonsters();
    int addMonster(const CActor actor);
    int findMonsterAt(const int x, const int y) const;
    void addHealth(const int hp);
    void addPoints(const int points);
    void addLife();
    bool read(FILE *sfile);
    bool write(FILE *tfile);
    int calcScoreLife() const;
    int secretTimer() const;
    const char *nextHint();

    friend class CGameMixin;
};
