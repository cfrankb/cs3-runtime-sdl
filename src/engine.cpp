#include "engine.h"
#include "menu.h"
#include "map.h"
#include "game.h"
#include "gamesfx.h"
#include "gamestats.h"
#include "statedata.h"
#include "attr.h"
#include "strhelper.h"
#include "states.h"
#include "menumanager.h"
#include "chars.h"
#include <cstring>
#include "gamemixin.h"

namespace Engine_Private
{
    constexpr const int FONT_SIZE = 8;
}

using namespace Engine_Private;

////////////////////////////////////////////////////////////////////

void Engine::drawPreScreen()
{
    const char t[] = "CLICK TO START";
    const int x = (getWidth() - strlen(t) * FONT_SIZE) / 2;
    const int y = (getHeight() - FONT_SIZE) / 2;
    //  m_font.drawText(m_renderer, t, SColor::WHITE, nullptr, x * SCALE, y * SCALE, SCALE, SCALE);
    drawFont(x, y, t, WHITE, 1, 1);
}

void Engine::drawLevelIntro()
{
    const int mode = m_game->mode();
    char t[32];
    switch (mode)
    {
    case CGame::MODE_LEVEL_INTRO:
        snprintf(t, sizeof(t), "LEVEL %.2d", m_game->level() + 1);
        break;
    case CGame::MODE_RESTART:
        if (m_game->lives() > 1)
        {
            snprintf(t, sizeof(t), "LIVES LEFT %.2d", m_game->lives());
        }
        else
        {
            strncpy(t, "LAST LIFE !", sizeof(t));
        }
        break;
    case CGame::MODE_GAMEOVER:
        strncpy(t, "GAME OVER", sizeof(t));
        break;
    case CGame::MODE_TIMEOUT:
        strncpy(t, "OUT OF TIME", sizeof(t));
        break;
    case CGame::MODE_CHUTE:
        snprintf(t, sizeof(t), "DROP TO LEVEL %.2d", m_game->level() + 1);
    };

    const int x = (getWidth() - strlen(t) * 2 * FONT_SIZE) / 2;
    const int y = (getHeight() - FONT_SIZE * 2) / 2;
    drawFont(x, y, t, YELLOW, 2, 2);
    // m_font.drawText(m_renderer, t, SColor::YELLOW, nullptr, x * SCALE, y * SCALE, SCALE_2X, SCALE_2X);

    if (mode == CGame::MODE_LEVEL_INTRO || mode == CGame::MODE_CHUTE)
    {
        const char *t = m_game->getMap().title();
        const int x = (getWidth() - strlen(t) * FONT_SIZE) / 2;
        drawFont(x, y + 3 * FONT_SIZE, t, WHITE);
        // m_font.drawText(m_renderer, t, SColor::WHITE, nullptr, x * SCALE, (y + 3 * FONT_SIZE) * SCALE, SCALE, SCALE);
    }

    if (mode != CGame::MODE_GAMEOVER && getWidth() >= MIN_WIDTH_FULL)
    {
        const char *hint = m_game->getHintText();
        const int x = (getWidth() - strlen(hint) * FONT_SIZE) / 2;
        const int y = getHeight() - FONT_SIZE * 4;
        drawFont(x, y, hint, CYAN);
        // m_font.drawText(m_renderer, hint, SColor::CYAN, nullptr, x * SCALE, y * SCALE, SCALE, SCALE);
    }

    // TODO: revisit
    // m_currentEvent = EVENT_NONE;
    // m_timer = TICK_RATE;
}

void Engine::drawTest()
{
    drawFont(16, 16, "COMING SOON 2026", YELLOW, 2, 2);
}

void Engine::drawSkillMenu()
{
    const int baseY = (getHeight() - m_menus->get(MENUID_USERS)->height()) / 2;
    const char *t = "GAME DIFFICULTY";
    int x = (getWidth() - strlen(t) * FONT_SIZE * 2) / 2;
    drawFont(x, baseY - 32, t, GRAY, 2, 2);
    drawMenu(*m_menus->get(MENUID_SKILLS), -1, baseY);
}

/**
 * @brief Draw Post Level Summary
 */
void Engine::drawLevelSummary()
{
    struct Text
    {
        std::string text;
        Color color;
        int scaleX;
        int scaleY;
    };
    std::vector<Text> listStr;
    char tmp[128];
    snprintf(tmp, sizeof(tmp), "LEVEL %.2d COMPLETED", m_game->level() + 1);
    if (getWidth() < MIN_WIDTH_FULL)
        listStr.emplace_back(Text{tmp, YELLOW, 1, 2});
    else
        listStr.emplace_back(Text{tmp, YELLOW, 2, 2});
    snprintf(tmp, sizeof(tmp), "Fruits Collected: %d %%", m_summary.ppFruits);
    listStr.emplace_back(Text{tmp, PINK, 1, 2});
    snprintf(tmp, sizeof(tmp), "Treasures Collected: %d %%", m_summary.ppBonuses);
    listStr.emplace_back(Text{tmp, ORANGE, 1, 2});
    snprintf(tmp, sizeof(tmp), "Secrets: %d %%", m_summary.ppSecrets);
    listStr.emplace_back(Text{tmp, GREEN, 1, 2});
    snprintf(tmp, sizeof(tmp), "Time Taken: %.2d:%.2d", m_summary.timeTaken / 60, m_summary.timeTaken % 60);
    listStr.emplace_back(Text{tmp, CYAN, 1, 2});

    if (m_state.m_countdown == 0 && ((m_ticks >> 3) & 1))
    {
        listStr.emplace_back(Text{"", BLACK, 1, 2});
        listStr.emplace_back(Text{"PRESS SPACE TO CONTINUE", LIGHTGRAY, 1, 2});
    }
    else
    {
        listStr.emplace_back(Text{"", BLACK, 1, 2});
        listStr.emplace_back(Text{"", BLACK, 1, 2});
    }

    int height = 0;
    for (const auto &item : listStr)
    {
        height += FONT_SIZE * item.scaleY + FONT_SIZE;
    }

    int y = (getHeight() - height) / 2;
    for (const auto &item : listStr)
    {
        const auto &str = item.text;
        const int x = (getWidth() - str.length() * FONT_SIZE * item.scaleX) / 2;
        drawFont(x, y, str.c_str(), item.color, item.scaleX, item.scaleY);
        y += FONT_SIZE * item.scaleY + FONT_SIZE;
    }
}

/**
 * @brief Initialize Post Level Summary Screen Data
 *
 */
void Engine::initLevelSummary()
{
    CGame &game = *m_game;
    const MapReport &report0 = game.originalMapReport();
    const MapReport report1 = game.currentMapReport();
    m_summary.ppFruits = report0.fruits ? 100 * (report0.fruits - report1.fruits) / report0.fruits : 100;
    m_summary.ppBonuses = report0.bonuses ? 100 * (report0.bonuses - report1.bonuses) / report0.bonuses : 100;
    m_summary.ppSecrets = report0.secrets ? 100 * (report0.secrets - report1.secrets) / report0.secrets : 100;
    m_summary.timeTaken = game.timeTaken();
}

/**
 * @brief Draw User Selection Screen
 *
 * @param bitmap
 */
void Engine::drawUserMenu()
{
    int baseY = (getHeight() - m_menus->get(MENUID_USERS)->height()) / 2;
    drawFont(32, baseY - 32, "SELECT DIAMOND HUNTER", GRAY, 1, 2);
    drawMenu(*m_menus->get(MENUID_USERS), 48, baseY);
}

void Engine::drawScores()
{
    int scaleX = 2;
    int scaleY = 2;
    if (getWidth() > 450)
    {
        scaleX = 4;
    }
    else if (getWidth() > 350)
    {
        scaleX = 3;
    }
    if (getHeight() >= 450)
    {
        scaleY = 3;
    }
    if (getHeight() >= 350)
    {
        scaleY = 3;
    }
    else if (getHeight() >= 300)
    {
        scaleY = 2;
    }

    // bitmap.fill(BLACK);
    char t[50];
    int y = 1;
    strncpy(t, "HALL OF HEROES", sizeof(t));
    int x = (getWidth() - strlen(t) * scaleX * FONT_SIZE) / 2;
    drawFont(x, y * FONT_SIZE, t, WHITE, scaleX, scaleY);
    y += scaleX;
    strncpy(t, std::string(strlen(t), '=').c_str(), sizeof(t) - 1);
    x = (getWidth() - strlen(t) * scaleX * FONT_SIZE) / 2;
    drawFont(x, y * FONT_SIZE, t, WHITE, scaleX, scaleY);
    y += scaleX;

    CGameMixin *mixin = m_gameMixin;
    for (int i = 0; i < static_cast<int>(MAX_SCORES); ++i)
    {
        Color color = i & INTERLINES ? LIGHTGRAY : DARKGRAY;
        if (mixin->recordScore() && mixin->scoreRank() == i)
        {
            color = YELLOW;
        }
        else if (mixin->scoreRank() == i)
        {
            color = CYAN;
        }

        const auto &hiScore = mixin->getHiScoreAtIndex(i);
        bool showCaret = (color == YELLOW) && (m_ticks & CARET_SPEED);
        snprintf(t, sizeof(t), " %.8d %.2d %s%c",
                 hiScore.score,
                 hiScore.level,
                 hiScore.name,
                 showCaret ? CHARS_CARET : '\0');
        drawFont(1, y * FONT_SIZE, t, color, scaleX / 2, scaleY / 2);
        y += scaleX / 2;
    }

    y += scaleX / 2;
    if (mixin->scoreRank() == INVALID)
    {
        strncpy(t, " SORRY, YOU DIDN'T QUALIFY.", sizeof(t));
        drawFont(0, y * FONT_SIZE, t, YELLOW, scaleX / 2, scaleY / 2);
    }
    else if (mixin->recordScore())
    {
        strncpy(t, "PLEASE TYPE YOUR NAME AND PRESS ENTER.", sizeof(t));
        x = (getWidth() - strlen(t) * FONT_SIZE) / 2;
        drawFont(x, y++ * FONT_SIZE, t, YELLOW, scaleX / 2, scaleY / 2);
    }
}

void Engine::drawHelpScreen()
{
    auto &helpText = m_gameMixin->helptext();
    int y = 0;
    for (size_t i = 0; i < helpText.size(); ++i)
    {
        const char *p = helpText[i].c_str();
        int x = 0;
        Color color = WHITE;
        if (p[0] == '~')
        {
            ++p;
            color = YELLOW;
        }
        else if (p[0] == '!')
        {
            ++p;
            x = (getWidth() - strlen(p) * FONT_SIZE) / 2;
        }
        drawFont(x, y * FONT_SIZE, p, color);
        ++y;
    }
}

/**
 * @brief Draw Ootion Menu
 */
void Engine::drawOptions()
{
    CMenu &menu = *m_menus->get(MENUID_OPTIONMENU);
    const int menuBaseY = 32;
    drawMenu(menu, 48, menuBaseY);
}

//////////////////////////////////////////////////////////////////////////

void Engine::resizeGameMenu()
{
    CMenu &menu = *m_menus->get(MENUID_GAMEMENU);
    if (getWidth() > MIN_WIDTH_FULL)
    {
        menu.setScaleX(2);
    }
    else
    {
        menu.setScaleX(1);
    }
}

void Engine::gatherSprites(std::vector<sprite_t> &sprites, const cameraContext_t &context)
{
    CGame &game = *m_game;
    CMap *map = &m_game->getMap();
    const int maxRows = getHeight() / TILE_SIZE;
    const int maxCols = getWidth() / TILE_SIZE;
    const int rows = std::min(maxRows, map->height());
    const int cols = std::min(maxCols, map->width());
    const int &mx = context.mx;
    const int &ox = context.ox;
    const int &my = context.my;
    const int &oy = context.oy;
    const std::vector<CActor> &monsters = game.getMonsters();
    for (const auto &monster : monsters)
    {
        const uint8_t &tileID = map->at(monster.x(), monster.y());
        if (monster.isWithin(mx, my, mx + cols + ox, my + rows + oy) &&
            m_animator->isSpecialCase(tileID))
        {
            const Pos pos = monster.pos();
            const uint8_t attr = map->getAttr(pos.x, pos.y);
            sprites.emplace_back(
                sprite_t{.x = monster.x(),
                         .y = monster.y(),
                         .tileID = tileID,
                         .aim = monster.getAim(),
                         .attr = attr});
        }
    }

    const std::vector<sfx_t> &sfxAll = m_game->getSfx();
    for (const auto &sfx : sfxAll)
    {
        if (sfx.isWithin(mx, my, mx + cols + ox, my + rows + oy))
        {
            sprites.emplace_back(sprite_t{
                .x = sfx.x,
                .y = sfx.y,
                .tileID = sfx.sfxID,
                .aim = AIM_NONE,
                .attr = 0,
            });
        }
    }
}

Engine::message_t Engine::getEventText(const int baseY)
{
    if (m_state.m_currentEvent == EVENT_SECRET)
    {
        return {
            .scaleX = 2,
            .scaleY = 1,
            .baseY = baseY,
            .color = CYAN,
            .lines{"SECRET !"},
        };
    }
    else if (m_state.m_currentEvent == EVENT_PASSAGE)
    {
        return {
            .scaleX = 2,
            .scaleY = 1,
            .baseY = baseY,
            .color = LIGHTGRAY,
            .lines{"PASSAGE"},
        };
    }
    else if (m_state.m_currentEvent == EVENT_EXTRA_LIFE)
    {
        return {
            .scaleX = 2,
            .scaleY = 1,
            .baseY = baseY,
            .color = GREEN,
            .lines{"EXTRA LIFE"},
        };
    }
    else if (m_state.m_currentEvent == EVENT_SUGAR_RUSH)
    {
        Color color;
        if ((m_ticks >> 3) & 1)
            color = LIME;
        else
            color = ORANGE;
        return {
            .scaleX = 2,
            .scaleY = 2,
            .baseY = baseY,
            .color = color,
            .lines{"SUGAR RUSH"},
        };
    }
    else if (m_state.m_currentEvent == EVENT_GOD_MODE)
    {
        return {
            .scaleX = 2,
            .scaleY = 1,
            .baseY = baseY,
            .color = BLACK,
            .lines{"GOD MODE!"},
        };
    }
    else if (m_state.m_currentEvent == EVENT_SUGAR)
    {
        char tmp[40];
        // const auto sugarLevel = m_game->stats().get(S_SUGAR_LEVEL);
        snprintf(tmp, sizeof(tmp), "YUMMY %d/%d", m_game->sugar(), CGame::MAX_SUGAR_RUSH_LEVEL);
        return {
            .scaleX = 2,
            .scaleY = 1,
            .baseY = baseY,
            .color = PURPLE,
            .lines{tmp},
        };
    }
    else if (RANGE(m_state.m_currentEvent, MSG0, MSGF))
    {
        const std::string tmp = m_game->getMap().states().getS(m_state.m_currentEvent);
        const auto list = split(tmp, '\n');
        const std::string &line1 = list[0];
        const std::string &line2 = list.size() > 1 ? list[1] : "";
        const std::string &line3 = list.size() > 2 ? list[2] : "";
        const int y = list.size() == 0 ? baseY - 14 : baseY + -8 * (std ::min(list.size(), (size_t)3));
        return {
            .scaleX = 1,
            .scaleY = 1,
            .baseY = y,
            .color = DARKGRAY,
            .lines{line1, line2, line3},
        };
    }
    else if (m_state.m_currentEvent == EVENT_RAGE)
    {
        return {
            .scaleX = 2,
            .scaleY = 1,
            .baseY = baseY,
            .color = DARKORANGE,
            .lines{"RAGE !!!"},
        };
    }
    else if (m_state.m_currentEvent == EVENT_FREEZE)
    {
        return {
            .scaleX = 2,
            .scaleY = 1,
            .baseY = baseY,
            .color = WHITE,
            .lines{"FREEZE TRAP"},
        };
    }
    else if (m_state.m_currentEvent == EVENT_TRAP)
    {
        return {
            .scaleX = 2,
            .scaleY = 1,
            .baseY = baseY,
            .color = RED,
            .lines{"TRAP"},
        };
    }
    else if (m_state.m_currentEvent == EVENT_EXIT_OPENED)
    {
        const char *text = (m_ticks >> 3) & 1 ? "EXIT DOOR IS OPENED" : "";
        return {
            .scaleX = 2,
            .scaleY = 1,
            .baseY = baseY - (int)TILE_SIZE,
            .color = SEAGREEN,
            .lines{text},
        };
    }
    else if (m_state.m_currentEvent == EVENT_SHIELD)
    {
        char tmp[40];
        snprintf(tmp, sizeof(tmp), "SHIELD %d%%", 25 * m_game->stats().get(S_SHIELD));
        return {
            .scaleX = 2,
            .scaleY = 1,
            .baseY = baseY,
            .color = LIME,
            .lines{tmp},
        };
    }
    else if (m_state.m_currentEvent == EVENT_BOAT)
    {
        return {
            .scaleX = 2,
            .scaleY = 1,
            .baseY = baseY,
            .color = BROWN,
            .lines{"BOAT FOUND"},
        };
    }
    else
    {
        LOGW("unhandled event type: 0x%.2x", m_state.m_currentEvent);
        return message_t{};
    }
}
