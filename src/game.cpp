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
#include <cstring>
#include <stdarg.h>
#include <string>
#include <vector>
#include "game.h"
#include "map.h"
#include "actor.h"
#include "sprtypes.h"
#include "tilesdata.h"
#include "maparch.h"
#include "shared/IFile.h"
#include "sounds.h"
#include "shared/interfaces/ISound.h"
#include "skill.h"

CMap map(30, 30);
uint8_t CGame::m_keys[MAX_KEYS];
static constexpr const char GAME_SIGNATURE[]{'C', 'S', '3', 'b'};

CGame::CGame()
{
    printf("staring up version: 0x%.8x\n", VERSION);

    m_monsterMax = DEFAULT_MAX_MONSTERS;
    m_monsters = new CActor[m_monsterMax];
    m_monsterCount = 0;
    m_health = 0;
    m_level = 0;
    m_lives = DEFAULT_LIVES;
    m_score = 0;
    m_skill = SKILL_EASY;
}

CGame::~CGame()
{
    if (m_monsters)
    {
        delete[] m_monsters;
    }

    if (m_sound)
    {
        delete m_sound;
    }
}

CMap &CGame::getMap()
{
    return map;
}

CActor &CGame::player()
{
    return m_player;
}

bool CGame::move(int aim)
{
    if (m_player.canMove(aim))
    {
        m_player.move(aim);
        consume();
        return true;
    }

    return false;
}

void CGame::consume()
{
    const uint8_t pu = m_player.getPU();
    const TileDef &def = getTileDef(pu);

    if (def.type == TYPE_PICKUP)
    {
        addPoints(def.score);
        m_player.setPU(TILES_BLANK);
        addHealth(def.health);
        playTileSound(pu);
    }
    else if (def.type == TYPE_KEY)
    {
        addPoints(def.score);
        m_player.setPU(TILES_BLANK);
        addKey(pu);
        addHealth(def.health);
        playSound(SOUND_KEY);
    }
    else if (def.type == TYPE_DIAMOND)
    {
        addPoints(def.score);
        m_player.setPU(TILES_BLANK);
        --m_diamonds;
        addHealth(def.health);
        playTileSound(pu);
    }
    else if (def.type == TYPE_SWAMP)
    {
        addHealth(def.health);
    }

    // apply flags
    if (def.flags & FLAG_EXTRA_LIFE)
    {
        addLife();
    }

    if (def.flags & FLAG_GODMODE)
    {
        m_godModeTimer = GODMODE_TIMER;
    }

    if (def.flags & FLAG_EXTRA_SPEED)
    {
        m_extraSpeedTimer = EXTRASPEED_TIMER;
    }

    // trigger key
    int x = m_player.getX();
    int y = m_player.getY();
    uint8_t attr = map.getAttr(x, y);
    if (attr != 0)
    {
        map.setAttr(x, y, 0);
        if (clearAttr(attr))
        {
            playSound(SOUND_0009);
        }
    }
}

bool CGame::init()
{
    // m_engine->init();
    // uint8_t *ptr = &levels_mapz;
    //  load levelArch index from memory
    // m_arch.fromMemory(ptr);
    return true;
}

bool CGame::loadLevel(bool restart)
{
    printf("loading level: %d ...\n", m_level + 1);
    setMode(restart ? MODE_RESTART : MODE_INTRO);

    // extract level from MapArch
    map = *(m_mapArch->at(m_level));

    printf("level loaded\n");

    Pos pos = map.findFirst(TILES_ANNIE2);
    printf("Player at: %d %d\n", pos.x, pos.y);
    m_player = CActor(pos, TYPE_PLAYER, AIM_DOWN);
    m_diamonds = map.count(TILES_DIAMOND);
    memset(m_keys, 0, sizeof(m_keys));
    m_health = DEFAULT_HEALTH;
    findMonsters();
    return true;
}

void CGame::nextLevel()
{
    addPoints(LEVEL_BONUS + m_health);
    if (m_level != m_mapArch->size() - 1)
    {
        ++m_level;
    }
    else
    {
        m_level = 0;
    }
}

void CGame::restartLevel()
{
    m_godModeTimer = 0;
    m_extraSpeedTimer = 0;
}

void CGame::restartGame()
{
    m_score = 0;
    m_lives = DEFAULT_LIVES;
    m_level = 0;
    m_nextLife = calcScoreLife();
    m_godModeTimer = 0;
    m_extraSpeedTimer = 0;
}

void CGame::setLevel(int levelId)
{
    m_level = levelId;
}

int CGame::level()
{
    return m_level;
}

void CGame::setMapArch(CMapArch *arch)
{
    m_mapArch = arch;
}

bool CGame::findMonsters()
{
    m_monsterCount = 0;
    for (int y = 0; y < map.hei(); ++y)
    {
        for (int x = 0; x < map.len(); ++x)
        {
            uint8_t c = map.at(x, y);
            const TileDef &def = getTileDef(c);
            if (def.type == TYPE_MONSTER ||
                def.type == TYPE_VAMPLANT ||
                def.type == TYPE_DRONE)
            {
                addMonster(CActor(x, y, def.type));
            }
        }
    }
    printf("%d monsters found.\n", m_monsterCount);
    return true;
}

int CGame::addMonster(const CActor actor)
{
    if (m_monsterCount >= m_monsterMax)
    {
        m_monsterMax += GROWBY_MONSTERS;
        CActor *t = new CActor[m_monsterMax];
        memcpy(reinterpret_cast<void *>(t), m_monsters, m_monsterCount * sizeof(CActor));
        delete[] m_monsters;
        m_monsters = t;
    }
    m_monsters[m_monsterCount++] = actor;
    return m_monsterCount;
}

int CGame::findMonsterAt(int x, int y)
{
    for (int i = 0; i < m_monsterCount; ++i)
    {
        const CActor &actor = m_monsters[i];
        if (actor.getX() == x && actor.getY() == y)
        {
            return i;
        }
    }
    return INVALID;
}

void CGame::manageMonsters(int ticks)
{
    const int speedCount = 9;
    bool speeds[speedCount];
    for (uint32_t i = 0; i < sizeof(speeds); ++i)
    {
        speeds[i] = i ? (ticks % i) == 0 : true;
    }

    const uint8_t dirs[] = {AIM_UP, AIM_DOWN, AIM_LEFT, AIM_RIGHT};
    std::vector<CActor> newMonsters;

    for (int i = 0; i < m_monsterCount; ++i)
    {
        CActor &actor = m_monsters[i];
        const uint8_t cs = map.at(actor.getX(), actor.getY());
        const TileDef &def = getTileDef(cs);
        if (!speeds[def.speed])
        {
            continue;
        }
        if (def.type == TYPE_MONSTER)
        {
            if (actor.isPlayerThere(actor.getAim()))
            {
                // apply health damages
                addHealth(def.health);
                if (def.ai & AI_STICKY)
                {
                    continue;
                }
            }

            int aim = actor.findNextDir();
            if (aim != AIM_NONE)
            {
                actor.move(aim);
                if (!(def.ai & AI_ROUND))
                {
                    continue;
                }
            }
            for (uint8_t i = 0; i < sizeof(dirs); ++i)
            {
                if (actor.isPlayerThere(dirs[i]))
                {
                    // apply health damages
                    addHealth(def.health);
                    if (def.ai & AI_FOCUS)
                    {
                        actor.setAim(dirs[i]);
                    }
                    break;
                }
            }
        }
        else if (def.type == TYPE_DRONE)
        {
            int aim = actor.getAim();
            if (aim < AIM_LEFT)
            {
                aim = AIM_LEFT;
            }
            if (actor.isPlayerThere(actor.getAim()))
            {
                // apply health damages
                addHealth(def.health);
            }
            if (actor.canMove(aim))
            {
                actor.move(aim);
            }
            else
            {
                aim ^= 1;
            }
            actor.setAim(aim);
        }
        else if (def.type == TYPE_VAMPLANT)
        {
            for (uint8_t i = 0; i < sizeof(dirs); ++i)
            {
                Pos p = CGame::translate(Pos{actor.getX(), actor.getY()}, dirs[i]);
                const uint8_t ct = map.at(p.x, p.y);
                const TileDef &defT = getTileDef(ct);
                if (defT.type == TYPE_PLAYER)
                {
                    // apply damage from config
                    addHealth(def.health);
                }
                else if (defT.type == TYPE_SWAMP)
                {
                    map.set(p.x, p.y, TILES_VAMPLANT);
                    newMonsters.push_back(CActor(p.x, p.y, TYPE_VAMPLANT));
                    break;
                }
                else if (defT.type == TYPE_MONSTER)
                {
                    int j = findMonsterAt(p.x, p.y);
                    if (j == INVALID)
                        continue;
                    CActor &m = m_monsters[j];
                    m.setType(TYPE_VAMPLANT);
                    map.set(p.x, p.y, TILES_VAMPLANT);
                    break;
                }
            }
        }
    }

    // moved here to avoid reallocation while using a reference
    for (auto const &monster : newMonsters)
    {
        addMonster(monster);
    }
}

void CGame::managePlayer(uint8_t *joystate)
{
    m_godModeTimer = std::max(m_godModeTimer - 1, 0);
    m_extraSpeedTimer = std::max(m_extraSpeedTimer - 1, 0);
    auto const pu = m_player.getPU();
    if (pu == TILES_SWAMP)
    {
        // apply health damage
        const TileDef &def = getTileDef(pu);
        addHealth(def.health);
    }
    uint8_t aims[] = {AIM_UP, AIM_DOWN, AIM_LEFT, AIM_RIGHT};
    for (uint8_t i = 0; i < 4; ++i)
    {
        uint8_t aim = aims[i];
        if (joystate[aim] && move(aim))
        {
            break;
        }
    }
}

Pos CGame::translate(const Pos &p, int aim)
{
    Pos t = p;

    switch (aim)
    {
    case AIM_UP:
        if (t.y > 0)
        {
            --t.y;
        }
        break;
    case AIM_DOWN:
        if (t.y < map.hei() - 1)
        {
            ++t.y;
        }
        break;
    case AIM_LEFT:
        if (t.x > 0)
        {
            --t.x;
        }
        break;
    case AIM_RIGHT:
        if (t.x < map.len() - 1)
        {
            ++t.x;
        }
    }

    return t;
}

bool CGame::hasKey(uint8_t c)
{
    for (uint32_t i = 0; i < sizeof(m_keys); ++i)
    {
        if (m_keys[i] == c)
        {
            return true;
        }
    }
    return false;
}

void CGame::addKey(uint8_t c)
{
    for (uint32_t i = 0; i < sizeof(m_keys); ++i)
    {
        if (m_keys[i] == c)
        {
            break;
        }
        if (m_keys[i] == '\0')
        {
            m_keys[i] = c;
            break;
        }
    }
}

bool CGame::goalCount() const
{
    return m_diamonds;
}

int CGame::clearAttr(uint8_t attr)
{
    int count = 0;
    for (int y = 0; y < map.hei(); ++y)
    {
        for (int x = 0; x < map.len(); ++x)
        {
            const uint8_t tileAttr = map.getAttr(x, y);
            if (tileAttr == attr)
            {
                ++count;
                const uint8_t tile = map.at(x, y);
                const TileDef &def = getTileDef(tile);
                if (def.type == TYPE_DIAMOND)
                {
                    --m_diamonds;
                }
                map.set(x, y, TILES_BLANK);
                map.setAttr(x, y, 0);
            }
        }
    }
    return count;
}

void CGame::addHealth(const int hp)
{
    if (hp > 0)
    {
        const int maxHealth = static_cast<int>(MAX_HEALTH) / (1 + 1 * m_skill);
        const int hpToken = hp / (1 + 1 * m_skill);
        m_health = std::min(m_health + hpToken, maxHealth);
    }
    else if (hp < 0 && !m_godModeTimer)
    {
        const int hpToken = hp * (1 + 2 * m_skill);
        m_health = std::max(m_health + hpToken, 0);
    }
}

void CGame::setMode(int mode)
{
    m_mode = mode;
}

int CGame::mode() const
{
    return m_mode;
}

bool CGame::isPlayerDead()
{
    return m_health == 0;
}

void CGame::killPlayer()
{
    m_lives = m_lives ? m_lives - 1 : 0;
}

bool CGame::isGameOver()
{
    return m_lives == 0;
}

int CGame::score()
{
    return m_score;
}

int CGame::lives()
{
    return m_lives;
}

int CGame::diamonds()
{
    return m_diamonds;
}

int CGame::health()
{
    return m_health;
}

uint8_t *CGame::keys()
{
    return m_keys;
}

void CGame::addPoints(const int points)
{
    m_score += points;
    if (m_score >= m_nextLife)
    {
        m_nextLife += calcScoreLife();
        addLife();
    }
}

void CGame::addLife()
{
    m_lives = std::min(m_lives + 1, static_cast<int>(MAX_LIVES));
}

int CGame::godModeTimer()
{
    return m_godModeTimer;
}

int CGame::playerSpeed()
{
    return m_extraSpeedTimer ? FAST_PLAYER_SPEED : DEFAULT_PLAYER_SPEED;
}

void CGame::getMonsters(CActor *&monsters, int &count)
{
    monsters = m_monsters;
    count = m_monsterCount;
}

CActor &CGame::getMonster(int i)
{
    return m_monsters[i];
}

bool CGame::read(FILE *sfile)
{
    auto readfile = [sfile](auto ptr, auto size)
    {
        return fread(ptr, size, 1, sfile) == 1;
    };

    // check signature/version
    uint32_t signature = 0;
    readfile(&signature, sizeof(signature));
    uint32_t version = 0;
    readfile(&version, sizeof(version));
    if (memcmp(GAME_SIGNATURE, &signature, sizeof(GAME_SIGNATURE)) != 0)
    {
        char sig[5] = {0, 0, 0, 0, 0};
        memcpy(sig, &signature, sizeof(signature));
        printf("savegame signature mismatched: %s\n", sig);
        return false;
    }
    if (version != VERSION)
    {
        printf("savegame version mismatched: 0x%.8x\n", version);
        return false;
    }

    // ptr
    uint32_t indexPtr = 0;
    readfile(&indexPtr, sizeof(indexPtr));

    // general information
    readfile(&m_lives, sizeof(m_lives));
    readfile(&m_health, sizeof(m_health));
    readfile(&m_level, sizeof(m_level));
    readfile(&m_nextLife, sizeof(m_nextLife));
    readfile(&m_diamonds, sizeof(m_diamonds));
    readfile(&m_godModeTimer, sizeof(m_godModeTimer));
    readfile(m_keys, sizeof(m_keys));
    readfile(&m_score, sizeof(m_score));
    readfile(&m_skill, sizeof(m_skill));
    m_player.read(sfile);

    // reading map
    CMap &map = getMap();
    if (!map.read(sfile))
    {
        return false;
    }

    // monsters
    decltype(m_monsterCount) count = 0;
    readfile(&count, sizeof(m_monsterCount));
    m_monsterCount = count;
    if (count > m_monsterMax)
    {
        if (m_monsters)
        {
            delete[] m_monsters;
        }
        m_monsterMax = m_monsterCount + GROWBY_MONSTERS;
        m_monsters = new CActor[m_monsterMax];
    }
    for (int i = 0; i < m_monsterCount; ++i)
    {
        m_monsters[i].read(sfile);
    }
    return true;
}

bool CGame::write(FILE *tfile)
{
    auto writefile = [tfile](auto ptr, auto size)
    {
        return fwrite(ptr, size, 1, tfile) == 1;
    };

    // writing signature/version
    writefile(&GAME_SIGNATURE, sizeof(GAME_SIGNATURE));
    uint32_t version = VERSION;
    writefile(&version, sizeof(version));

    // ptr
    uint32_t indexPtr = 0;
    writefile(&indexPtr, sizeof(indexPtr));

    // write general information
    writefile(&m_lives, sizeof(m_lives));
    writefile(&m_health, sizeof(m_health));
    writefile(&m_level, sizeof(m_level));
    writefile(&m_nextLife, sizeof(m_nextLife));
    writefile(&m_diamonds, sizeof(m_diamonds));
    writefile(&m_godModeTimer, sizeof(m_godModeTimer));
    writefile(m_keys, sizeof(m_keys));
    writefile(&m_score, sizeof(m_score));
    writefile(&m_skill, sizeof(m_skill));
    m_player.write(tfile);

    // saving map
    CMap &map = getMap();
    map.write(tfile);

    // monsters
    writefile(&m_monsterCount, sizeof(m_monsterCount));
    for (int i = 0; i < m_monsterCount; ++i)
    {
        m_monsters[i].write(tfile);
    }
    return true;
}

void CGame::setLives(int lives)
{
    m_lives = lives;
}

void CGame::playSound(const int id) const
{
#ifdef __EMSCRIPTEN__
    (void)id;
#else
    if (id != SOUND_NONE && m_sound != nullptr)
    {
        m_sound->play(id);
    }
#endif
}

void CGame::playTileSound(int tileID) const
{
    int snd = SOUND_NONE;
    switch (tileID)
    {
    case TILES_FLOWERS_2:
    case TILES_CHEST:
    case TILES_NECKLESS:
        snd = SOUND_COIN1;
        break;
    case TILES_FRUIT1:
    case TILES_APPLE:
        snd = SOUND_GRUUP;
        break;
    }
    playSound(snd);
}

void CGame::attach(ISound *s)
{
    m_sound = s;
}

uint8_t CGame::skill() const
{
    return m_skill;
}

void CGame::setSkill(const uint8_t v)
{
    m_skill = v;
    m_nextLife = calcScoreLife();
}

int CGame::calcScoreLife()
{
    return SCORE_LIFE * (1 + m_skill);
}