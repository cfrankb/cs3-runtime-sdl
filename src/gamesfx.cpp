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

#include <algorithm>

#include "gamesfx.h"

// layer base/count from the contiguous imageID runs of
// cs3-data-private/pixels/sfx.idx; sequences without an explicit
// EffectBack/EffectFront suffix are front-layer.
// ponytail: speed/ttl are design placeholders until per-effect tuning.
std::vector<fxData_t> g_fxData = {
    {.id = FX_BLUE_LIGHTNING, .sheet = 0, .speed = 6, .flags = FX_ONESHOT,
     .ttl = 0, .w = 32, .h = 48, .back = {0, 0}, .front = {0, 6}, .onEnd = nullptr},
    {.id = FX_FIRE_COLUMN, .sheet = 0, .speed = 6, .flags = FX_ONESHOT,
     .ttl = 0, .w = 32, .h = 48, .back = {0, 0}, .front = {6, 8}, .onEnd = nullptr},
    {.id = FX_MUSHROOM_EXPLOSION, .sheet = 0, .speed = 6, .flags = FX_ONESHOT,
     .ttl = 0, .w = 32, .h = 48, .back = {0, 0}, .front = {14, 8}, .onEnd = nullptr},
    {.id = FX_ICE_SPIKES, .sheet = 0, .speed = 6, .flags = FX_ONESHOT,
     .ttl = 0, .w = 32, .h = 48, .back = {0, 0}, .front = {22, 6}, .onEnd = nullptr},
    {.id = FX_SMOKE_BOMB, .sheet = 0, .speed = 6, .flags = FX_ONESHOT,
     .ttl = 0, .w = 32, .h = 32, .back = {0, 0}, .front = {28, 6}, .onEnd = nullptr},
    {.id = FX_EXPLOSION, .sheet = 0, .speed = 6, .flags = FX_ONESHOT,
     .ttl = 0, .w = 48, .h = 48, .back = {34, 6}, .front = {40, 6}, .onEnd = nullptr},
};

// filled by the sheet loader from the sfx.png obLT metadata (imageID order)
std::vector<tileinfo_t> g_fxSheet_data;

const fxData_t *fxDataAt(const int effectID)
{
    if (effectID < 0 || static_cast<size_t>(effectID) >= g_fxData.size())
        return nullptr;
    return &g_fxData[static_cast<size_t>(effectID)];
}

uint16_t fxFrameIndex(const fx_t &fx, const fxData_t &def, const fx_layer_t &layer)
{
    if (layer.count == 0)
        return layer.base;
    const uint32_t step = def.speed ? def.speed : 1;
    const uint32_t seq = fx.timer / step;
    const uint32_t idx = (def.flags & FX_REPEATABLE)
                             ? seq % layer.count
                             : std::min<uint32_t>(seq, layer.count - 1);
    return static_cast<uint16_t>(layer.base + idx);
}
