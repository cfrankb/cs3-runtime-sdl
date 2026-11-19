/*
    Test / verification driver for the runtime boss data loader.

    Loads the real techdocs/bosses/bosses.ini + data/*.json at runtime and
    checks every field against the values produced by the old static
    generator (src/bossdata.cpp). Also exercises getHitboxes() for a few
    known frames.

    Usage:
        boss_runtime_test [iniDir] [hitboxDir]

    Defaults point at the project's own data so no copying is needed.
*/
#include "bossdata_runtime.h"

#include "sounds.h"
#include "tilesdata.h"

#include <cstdio>
#include <cstring>
#include <string>

#include "../../../src/logger.h"

static int g_failures = 0;
static int g_checks = 0;

static void check(const char* what, bool ok)
{
    ++g_checks;
    if (!ok)
    {
        ++g_failures;
        LOGE("  FAIL: %s\n", what);
    }
}

template <typename A, typename B>
static void checkEq(const char* what, A got, B want)
{
    ++g_checks;
    if (got != want)
    {
        ++g_failures;
        LOGE("  FAIL: %s -> got %d want %d\n", what, (int)got, (int)want);
    }
}

static void checkStr(const char* what, const std::string& got, const std::string& want)
{
    ++g_checks;
    if (got != want)
    {
        ++g_failures;
        LOGE("  FAIL: %s -> got \"%s\" want \"%s\"\n", what, got.c_str(), want.c_str());
    }
}

int main(int argc, char** argv)
{
    const char* iniDir = (argc > 1) ? argv[1] : "/home/cfrankb/toolkit/gcc/cs3-runtime-sdl/techdocs/bosses";
    const char* hitboxDir = (argc > 2) ? argv[2] : "/home/cfrankb/toolkit/gcc/cs3-runtime-sdl/techdocs/bosses/data";

    std::printf("loadBossData(%s, %s)\n", iniDir, hitboxDir);
    int rc = loadBossData(iniDir, hitboxDir);
    if (rc != 0)
    {
        LOGE("loadBossData returned %d\n", rc);
        return 2;
    }

    std::printf("g_bossCount=%d g_hitboxCount=%d\n", g_bossCount, g_hitboxCount);
    check("boss count == 4", g_bossCount == 4);
    check("hitbox count == 27", g_hitboxCount == 27);

    // --- per-boss field checks (values copied from the static generator) ---
    const bossData_t* demon = getBossData(0xb0);
    const bossData_t* ghost = getBossData(0xb1);
    const bossData_t* harpy = getBossData(0xb2);
    const bossData_t* beholder = getBossData(0xb3);
    check("found Mr. Demon", demon != nullptr);
    check("found Ghost", ghost != nullptr);
    check("found Harpy", harpy != nullptr);
    check("found Beholder", beholder != nullptr);

    if (demon)
    {
        checkStr("demon.name", std::string(demon->name), std::string("Mr. Demon"));
        checkEq("demon.speed", demon->speed, 3);
        checkEq("demon.speed_anime", demon->speed_anime, 3);
        checkEq("demon.hp", demon->hp, 64);
        checkEq("demon.type", demon->type, 0xb0);
        checkEq("demon.score", demon->score, 1000);
        checkEq("demon.damage", demon->damage, 16);
        checkEq("demon.damage_attack", demon->damage_attack, 16);
        checkEq("demon.damage_special1", demon->damage_special1, 16);
        checkEq("demon.damage_special2", demon->damage_special2, 16);
        checkEq("demon.flags", demon->flags, 0x80000000u);
        checkEq("demon.path", demon->path, (uint32_t)BossData::Path::ASTAR);
        checkEq("demon.bullet", demon->bullet, TILES_FIREBALL);
        checkEq("demon.bullet_rate", demon->bullet_rate, 15);
        checkEq("demon.bullet_algo", demon->bullet_algo, (uint8_t)BossData::Path::NONE);
        checkEq("demon.bullet_sound", demon->bullet_sound, SOUND_SHOOT3);
        checkEq("demon.bullet_ttl", demon->bullet_ttl, BossData::NoTTL);
        checkEq("demon.attack_sound", demon->attack_sound, SOUND_DINOSAUR3);
        checkEq("demon.distance_chase", demon->distance_chase, 10);
        checkEq("demon.distance_pursuit", demon->distance_pursuit, 15);
        checkEq("demon.distance_attack", demon->distance_attack, 8);
        check("demon.is_goal", demon->is_goal == true);
        check("demon.show_details", demon->show_details == true);
        checkEq("demon.color_hp", (uint32_t)demon->color_hp, (uint32_t)ORANGE);
        checkEq("demon.color_name", (uint32_t)demon->color_name, (uint32_t)RED);
        checkEq("demon.aims", demon->aims, 1);
        checkEq("demon.moving.base", demon->moving.base, 0);
        checkEq("demon.moving.lenght", demon->moving.lenght, 4);
        checkEq("demon.attack.base", demon->attack.base, 4);
        checkEq("demon.attack.lenght", demon->attack.lenght, 8);
        checkEq("demon.hurt.base", demon->hurt.base, 12);
        checkEq("demon.hurt.lenght", demon->hurt.lenght, 4);
        checkEq("demon.death.base", demon->death.base, 16);
        checkEq("demon.death.lenght", demon->death.lenght, 6);
        checkEq("demon.idle.base", demon->idle.base, 0); // @moving
        checkEq("demon.idle.lenght", demon->idle.lenght, 4);
        checkEq("demon.hitbox", demon->hitbox.x, 2);
        checkEq("demon.hitbox.y", demon->hitbox.y, 6);
        checkEq("demon.hitbox.width", demon->hitbox.width, 4);
        checkEq("demon.hitbox.height", demon->hitbox.height, 2);
    }

    if (ghost)
    {
        checkEq("ghost.speed", ghost->speed, 4);
        checkEq("ghost.hp", ghost->hp, 64);
        checkEq("ghost.type", ghost->type, 0xb1);
        checkEq("ghost.flags", ghost->flags, 0u);
        checkEq("ghost.path", ghost->path, (uint32_t)BossData::Path::LOS);
        checkEq("ghost.bullet", ghost->bullet, TILES_BLANK);
        checkEq("ghost.distance_pursuit", ghost->distance_pursuit, 20);
        checkEq("ghost.moving.base", ghost->moving.base, 25);
        checkEq("ghost.moving.lenght", ghost->moving.lenght, 5);
        checkEq("ghost.attack.base", ghost->attack.base, 30);
        checkEq("ghost.death.base", ghost->death.base, 44);
        checkEq("ghost.death.lenght", ghost->death.lenght, 0);
        checkEq("ghost.idle.base", ghost->idle.base, 22);
    }

    if (harpy)
    {
        checkEq("harpy.type", harpy->type, 0xb2);
        checkEq("harpy.hp", harpy->hp, 128);
        checkEq("harpy.path", harpy->path, (uint32_t)BossData::Path::BFS);
        checkEq("harpy.bullet", harpy->bullet, TILES_BULLETY1);
        checkEq("harpy.bullet_ttl", harpy->bullet_ttl, 20);
        checkEq("harpy.aims", harpy->aims, 1);
        checkEq("harpy.attack.base", harpy->attack.base, 44);
        checkEq("harpy.attack.lenght", harpy->attack.lenght, 8);
        checkEq("harpy.death.base", harpy->death.base, 52);
        checkEq("harpy.idle.base", harpy->idle.base, 58);
        checkEq("harpy.moving.base", harpy->moving.base, 58); // @idle
        checkEq("harpy.hurt.base", harpy->hurt.base, 62);
    }

    if (beholder)
    {
        checkEq("beholder.type", beholder->type, 0xb3);
        checkEq("beholder.flags", beholder->flags, 0x40000000u);
        checkEq("beholder.damage_special1", beholder->damage_special1, 20);
        checkEq("beholder.aims", beholder->aims, 4);
        checkEq("beholder.sheet", beholder->sheet, 1);
        checkEq("beholder.attack.base", beholder->attack.base, 0); // sheet 1 resets frame
        checkEq("beholder.attack.lenght", beholder->attack.lenght, 8);
        checkEq("beholder.moving.base", beholder->moving.base, 68);
        checkEq("beholder.idle.base", beholder->idle.base, 68); // @moving
    }

    // --- hitbox accessor checks (spriteID = sheet*1000 + frame + id) ---
    // Sheet 0 hitboxes come from Harpy's attack (base 44, sprite_id 2..4
    // -> frames 46, 47, 48). Mr. Demon's own animations carry no hitbox files.
    const sprite_hitbox_t* hb = getHitboxes(0, 46);
    check("getHitboxes(harpy attack frame 46) != null", hb != nullptr);
    if (hb)
        checkEq("harpy attack hitbox spriteID", hb->spriteID, 46);

    // Demon attack frame 4 has no hitbox file -> null (matches static data).
    check("getHitboxes(demon attack frame 4) == null", getHitboxes(0, 4) == nullptr);

    // Beholder attack (sheet 1, base 0, 8 frames): frame 0 -> spriteID 1000.
    const sprite_hitbox_t* bhb = getHitboxes(1, 0);
    check("getHitboxes(beholder attack frame 0, sheet 1) != null", bhb != nullptr);
    if (bhb)
        checkEq("beholder attack hitbox spriteID", bhb->spriteID, 1000);

    // Unknown type / frame must return null.
    check("getBossData(0xFF) == null", getBossData(0xFF) == nullptr);
    check("getHitboxes(99, 99) == null", getHitboxes(99, 99) == nullptr);

    std::printf("\n%d checks, %d failures\n", g_checks, g_failures);
    freeBossData();
    return g_failures == 0 ? 0 : 1;
}
