# Audit: src/actor.cpp
Date: 2026-11-17

## Summary
Overall the file is small and mostly clean (unique_ptr-owned path, deleted copy ops, symmetric serialization), but the aim-table lookup in `findNextDir` is a live out-of-bounds read when `m_aim == AIM_NONE` (0xff), which game code stores in actors routinely. Secondary concerns: the `m_frame`/`m_algo` union aliasing crossed through `startPath`, an enum→bool success check that treats `NoValidPath` as success, and silent deserialization failures. Counts: 0 Critical (1 downgraded to Low, see Triage), 1 High, 5 Medium, 7 Low.

Status (re-verified against `src/actor.cpp` on 2026-11-18, `cs3-runtime-tests` green): the downgraded Critical is **closed** by the range guard now at `actor.cpp:289-291`; the High is **closed** (BUG-H12). Open: 5 Medium (incl. unvalidated `m_aim` from `read()`) + 7 Low.

## Findings

### Critical
- L288: [bug/ub] **FIXED 2026-11-17 (commit `5ca9422`; verified 2026-11-18: `const int aim = m_aim; if (aim >= TOTAL_AIMS) return AIM_NONE;` at actor.cpp:289-291).** Original finding: `g_aims[aim * TOTAL_AIMS + i]` reads out of bounds when `m_aim == AIM_NONE` (0xff): index `255*4+i` = 1020..1023 into a 16-entry array. **Triaged 2026-11-17: downgraded to Low (defense-in-depth).** `AIM_NONE` is a placeholder for inactive actors (boulders/icecubes via game.cpp:637, settled ice via handleIceCube's own `AIM_NONE` guard at game_ai.cpp:671); those types never dispatch to `handleMonster`/`handleMonsterV3`, and `pushChain` only ever sets valid aims. Remaining theoretical paths: a monster tile carrying an ATTR_RESPAWN attr (respawn at game_ai.cpp:79 never sets aim — level-data question) and raw `m_aim` from a corrupt save (L444, still unvalidated in `read()` — tracked as a separate Medium).

### High
**FIXED 2026-11-17 (BUG-H12):** union split into two `uint16_t` members; move ctor/assign carry both. **L596: [bug] `m_algo = algo` in `startPath` clobbers `m_frame`: `m_frame`/`m_algo` share a `uint16_t` union (actor.h L147-151), and the engine renders `.frame = monster.frame()` for all actors (engine.cpp:375, gamemixin.cpp:701), so any actor that both animates and follows a path renders a frame index taken from the path-algo id (potential animation-table overrun in the renderer, needs verification). Egg hatch (game_ai.cpp:809-822) relies on the overwrite being sequential, but nothing enforces that discipline. Fix: split into two members unless the union is provably safe for every actor class; at minimum document the invariant at L596.

### Medium
**FIXED 2026-11-18: `== CPath::Result::MoveSuccesful`; verified all callers
(game_ai.cpp:200, 557) discard the bool, so zero runtime impact.**
- L604: [bug] `startPath` returns `CPath::Result` implicitly converted to `bool`: `NoValidPath` (2) and `NotConfigured` (3) are truthy, so a failed path search reports success. Fix: `return m_path->followPath(...) == CPath::Result::MoveSuccesful;` (needs verification of intended semantics — game_ai.cpp:855-857 does distinguish the results).
**FIXED 2026-11-18 (maintainer ruling: tileID is `uint16_t`, downstream of
the `uint16_t` layer tiles). `m_pu`, `getPU`/`setPU`, `tileAt` and the four
receiving locals widened; wire record widened → `ENGINE_VERSION` +
`EXPECTED_VERSION` bumped together to `0x0200_0010`. `addKey`/`key_t.tiles`
key storage left `uint8_t` (not in ruling scope).** Follow-up (the widened
types now pass hostile high cell values through to the unbounded
`getTileDef(int)`): CLOSED 2026-11-18 by maintainer ruling — engine-level
`CGame::sanitiseMapTiles()` at both map ingest points (`loadLevel`,
savegame `read`) scans every layer and replaces cells at or past their
consumer table (`TILES_TOTAL_COUNT` for `baseID()==0` layers feeding
`tileDefs`, `TOTAL_TILE_COUNT` for the rest feeding the animator tables
and `g_layerdata`) with `TILES_BLANK`/no-sprite; one `LOGW` carries the
total count; `CMap`/`CLayer` untouched by ruling. Pinned by
`t_game::testMapTileSanitiser`.
- L184, L229, L236, L347: [bug] `const uint8_t c = map.at(...)` / `m_pu = map.at(...)` truncate `CMap::at`'s `uint16_t` return (map.h:127). Latent while tile IDs stay < 256 (none found in tilesdefs.h today, needs verification against level archives); fix: use `uint16_t` locals or an explicit checked narrowing. `m_pu` itself is `uint8_t` (actor.h:153), so full fix touches the member type.
- L269-273: [bug] `setPos()` teleports without moving the map cell or re-syncing `m_pu`; a subsequent `move()` (L230) then writes the stale `m_pu` from the old cell into the new one.
**DOCUMENTED 2026-11-18 (maintainer ruling).** Not a bug: `setPos()`
deliberately moves only the actor and never touches the map — a use
case distinct from `move()` (repositioning where cells must stay put;
the six `t_gamemixin.cpp` callers depend on exactly that, e.g. placing
the player outside the main-layer swap path). Actors in production are
placed by the ctors (`m_pu = TILES_BLANK` spawn-absorb model) and
restored as a consistent `m_x/m_y/m_pu` triple from saves, so the
bookkeeping invariant is never routed through this setter. `actor.h`/
`actor.cpp` now document the contract. (A swap-taking-over variant was
tried and reverted the same day: it threw out-of_range in the viewport
tests and contradicted the intended semantics.)
- L405-409: [dead] Null check after `std::make_unique` is unreachable — `make_unique` throws `std::bad_alloc`, it never returns null. As written, allocation failure escapes as an exception instead of the function's `false` contract. Delete the check (L402 `if (!m_path)` already guards). **FIXED 2026-11-18: dead check deleted, comment notes the never-null contract; `bad_alloc` still escapes as an exception (unchanged, engine-wide convention).**
- L436-459, L417-425: [error-handling] Every failed field read returns `false` with no `LOGE`/`LOGW`, and a mid-record failure leaves the actor half-overwritten (fields mutated before the failure, `m_path` possibly replaced at L404) with no rollback or diagnostic. **FIXED 2026-11-18 (diagnostics half): both `CActor::read` and `CActor::readPath` read-lambdas now `LOGE` the failed size + stream offset, covering all field reads without per-field boilerplate (offset + fixed field order localizes the field; existing truncated/forged-record tests in `t_actor` exercise the path and stay green).** No rollback added: callers treat a `false` as fatal for the whole savegame load, so a half-read actor never enters play.

### Low
- L69+L78, L85+L92: [shrink] `m_path` is set twice — once in the member-init list, again in the ctor body. Remove the body assignments.
- L85-97: [bug] Second ctor lacks the aim validation the first ctor performs (L71-72); `AIM_UP`-out-of-range or arbitrary aims pass silently. Route both ctors through one validating init.
**FIXED 2026-11-18: `readCommon` rejects `m_aim >= TOTAL_AIMS && m_aim !=
AIM_NONE` with LOGE (AIM_NONE stays legal — saved placeholder for inactive
actors). Pinned in `t_actor` forged-record aim cases.**
- L444: [error-handling] `m_aim` is deserialized raw with no range check, unlike the ctor which validates aim values; a corrupt save yields an arbitrary `m_aim` (0-255) that feeds the L288 table index. Add a post-read validity check with `LOGE` (defense in depth for the Critical finding).
- L561-565: [error-handling] `move(int16_t, int16_t)` silently wrapped out-of-range/negative coordinates (y=-1 → 255, teleporting the actor off-map). **FIXED 2026-11-18 (maintainer ruling: "maps don't wrap; negative positions don't make sense"; throw variant rejected as per-move cost — "simply cap the coordinates"):** coordinates are now capped to the representable cell range [0, 255] via `std::clamp`; in-range behavior unchanged and the redundant `& 0xff` magic mask (the audit's buckets had no magic-number category, it surfaced only via this finding) is gone. CPath callers pass structurally in-grid coordinates, so the cap never engages in production. Pinned by `t_actor::test_actor_move_bounds` (in-range store; negative x/y and oversized coordinates cap to 0 / 255).
- L527-530: [dead] Free `operator^=(JoyAim&, int)` has no callers anywhere in the repo (only its declaration in joyaim.h and this definition). Delete both.
- L550-552: [shrink] `std::abs(int)` is used without including `<cstdlib>` (works via transitive includes only); also L537 `const Pos` return-by-value top-level const is noise.

## Notes
- `CGame::getGame()` (game.cpp:1683-1685) is a static singleton, so the `game->scanPos` deref at L175-176 is safe; `CGame::translate` (game.cpp:779) clamps at map edges, so all `map.at` indices from `canMove`/`tileAt`/`move` are in range.
- The union aliasing lives in actor.h but every write site (`setAlgo`, `startPath` L596, serialization L448/L507 via `m_algo`) and read site (`algo()` in `followPath` L574) straddles the two files — any fix must cover both. Runtime-verify by rendering a monster whose `algo()` != `BossData::Path::NONE` and checking its animation frame.
- L581 (`m_ttl == 0 && isBulletType` → `m_path.reset()`) pairs with game_ai.cpp:869; non-bullet actors with `ttl == 0` keep following their path here by design — confirm with path/ttl handling in ai_path.cpp before "fixing".
- Serialization is symmetric (same `sizeof` per field on both sides, `JoyAim` is `uint8_t`), so no format bug found; only the missing validation is an issue.
