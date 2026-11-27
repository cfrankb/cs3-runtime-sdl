# Projectile Movement Specification: Natural & Tactical Bullet Trajectories

## Context & Constraints
* **World Model:** Discrete tile collision grid (`TILE_SIZE` in pixels/world units), with projectiles moving in continuous floating-point space (`float x, y`) decoupled from actor tile snapping.
* **Existing Primitives:**
  * `isTileBlocked(int tileX, int tileY)` (Returns `true` if the tile is solid wall/obstacle).
  * `hasLOS(TilePos from, TilePos to)` (Line-of-sight check; already implemented).
  * Target state: `player.pos` (`float x, y`) and `player.vel` (`float vx, vy`).
* **Goal:** Replace rigid 4-way/8-way grid-locked projectiles with fluid, readable, and distinct projectile behaviors across Simple Mobs, Intermediate Mobs, and Bosses.

---

## Task 1: Decouple Projectile Coordinates from the Tile Grid
Ensure all bullets store continuous coordinates and velocity vectors while colliding cleanly against the integer tile map.

1. **Projectile State Struct:**
   * Position: `float m_x, m_y`
   * Velocity: `float m_vx, m_vy`
   * Speed: `float m_speed`
   * Behavior Type: `enum class BulletType { Straight, LeadTargeted, SoftHoming, Ricochet, Wave }`
   * Lifetime / Bounce Counters: `int m_bouncesRemaining = 0`, `float m_age = 0.0f`
2. **Grid Conversion Helper:**
   * Convert continuous position to tile indices via `tileX = static_cast<int>(m_x / TILE_SIZE)` and `tileY = static_cast<int>(m_y / TILE_SIZE)`.

---

## Task 2: Implement Spawn Trajectory Calculators
Compute initial `(m_vx, m_vy)` at spawn time based on the firing entity's tier:

1. **Direct Aim (Tier 1 Simple Mobs):**
   * Compute direction vector `d = normalize(player.pos - emitter.pos)`.
   * Set `vel = d * m_speed`.
2. **Predictive Lead-Targeting (Tier 2 Intermediate Mobs & Bosses):**
   * Estimate time-of-impact: `dist = length(player.pos - emitter.pos)` and `t = dist / m_speed`.
   * Predict future position: `predictedPos = player.pos + (player.vel * t * leadFactor)` (where `leadFactor` is `0.5f`–`0.85f` so shots feel smart without being unfair).
   * **Wall-Clip Guard:** If `isTileBlocked` at `predictedPos`, clamp `predictedPos` back to `player.pos`.
   * Set `vel = normalize(predictedPos - emitter.pos) * m_speed`.
3. **Spread / Fan Volleys (Bosses):**
   * Given a base aim angle `theta = atan2(dy, dx)`, spawn `N` projectiles offset by `angleOffset = (i - (N - 1) / 2.0f) * spreadRadians`.
   * Set `vel = { cos(theta + angleOffset) * m_speed, sin(theta + angleOffset) * m_speed }`.

---

## Task 3: Implement Per-Tick Update & Steering Behaviors
Update projectile position each frame/tick (`dt`) according to its `BulletType`:

1. **Soft Homing (Turn-Rate Limited Boss Orbs):**
   * Compute desired direction `desiredDir = normalize(player.pos - pos)`.
   * Compute current direction `currDir = normalize(vel)`.
   * Rotate `currDir` toward `desiredDir` by at most `maxTurnRate * dt` radians (e.g., `1.5` to `2.5` rad/sec).
   * Update `vel = newDir * m_speed`.
   * *Note:* Limiting angular velocity guarantees projectiles overshoot sharp dodges in a wide arc rather than making impossible 180° hairpin turns.
2. **Sine-Wave Oscillation (Magical / Energy Projectiles):**
   * Maintain forward axis `forward` and perpendicular axis `perp = { -forward.y, forward.x }`.
   * Increment `m_age += dt`.
   * Apply lateral offset velocity: `effectiveVel = (forward * m_speed) + (perp * cos(m_age * frequency) * amplitude)`.

---

## Task 4: Implement Tile-Edge Ricochet & Wall Collision
Handle wall impacts cleanly without expensive raycasting by stepping X and Y axes independently:

1. **Axis-Separated Integration:**
   * Store `prevTileX = static_cast<int>(m_x / TILE_SIZE)` and `prevTileY = static_cast<int>(m_y / TILE_SIZE)`.
   * Advance X: `m_x += m_vx * dt`. Check `currTileX = static_cast<int>(m_x / TILE_SIZE)`.
     * If `currTileX != prevTileX && isTileBlocked(currTileX, prevTileY)`:
       * If `m_bouncesRemaining > 0`: negate `m_vx = -m_vx`, revert `m_x` out of the wall, and decrement `m_bouncesRemaining`.
       * Else: destroy projectile / trigger impact effect.
   * Advance Y: `m_y += m_vy * dt`. Check `currTileY = static_cast<int>(m_y / TILE_SIZE)`.
     * If `currTileY != prevTileY && isTileBlocked(currTileX, currTileY)`:
       * If `m_bouncesRemaining > 0`: negate `m_vy = -m_vy`, revert `m_y` out of the wall, and decrement `m_bouncesRemaining`.
       * Else: destroy projectile / trigger impact effect.

---

## Acceptance Criteria
1. **360° Free Flight:** Projectiles travel smoothly along arbitrary angles across the tile grid without snapping to 4-way or 8-way directions.
2. **Fair Lead Shots:** Tier 2 lead-targeted shots aim ahead of a moving player, but never aim inside solid walls when the player walks into a corner.
3. **Dodgable Homing:** Soft-homing boss projectiles curve smoothly toward the player and overshoot in a wide arc if the player sidesteps perpendicular to the projectile's path.
4. **Clean Tile Ricochets:** Ricocheting projectiles reflect accurately off vertical walls (`m_vx` inverted) and horizontal walls (`m_vy` inverted) up to `m_bouncesRemaining` times before despawning.
