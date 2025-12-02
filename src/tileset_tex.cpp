/*
    cs3-runtime-sdl
    Copyright (C) 2025  Francois Blanchette

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

#include "tileset_tex.h"
#include "shared/Frame.h"
#include "shared/FrameSet.h"
#include "shared/FileWrap.h"
#include "shared/PngMagic.h"
#include "logger.h"

TileSet::TileSet()
    : m_texture(nullptr), m_frame{nullptr}
{
    m_sx = 0;
    m_sy = 0;
    m_scale = 2;
}

TileSet::~TileSet()
{
    if (m_texture)
        SDL_DestroyTexture(m_texture);

    if (m_frame)
        delete m_frame;

    for (auto &[mask, texture] : m_textures)
    {
        SDL_DestroyTexture(texture);
    }
}

bool TileSet::load(SDL_Renderer *renderer, const std::string_view &filename, const int sx, const int sy)
{
    LOGI("extracting texture from %s", filename.data());
    CFileWrap file;
    if (!file.open(filename, "rb"))
    {
        LOGE("can't open %s", filename.data());
        return false;
    }

    CFrameSet set;
    if (!parsePNG(set, file, 0, true))
    {
        LOGE("fail to parse %s", filename.data());
        return false;
    }
    file.close();

    if (set.getSize() == 0)
        return false;

    m_frame = set.removeAt(0); // detach frame

    return load(renderer, m_frame, sx, sy);
}

bool TileSet::load(SDL_Renderer *renderer, CFrame *frame, const int sx, const int sy)
{
    m_frame = frame;
    m_sx = sx;
    m_sy = sy;
    m_texture = createTexture(renderer, m_frame);
    if (!m_texture)
    {
        LOGE("create texture failed. sx: %d sy: %d", sx, sy);
        return false;
    }

    SDL_SetTextureScaleMode(m_texture, SDL_SCALEMODE_NEAREST);

    return saveTiles(m_texture, m_tiles, m_frame);
}

bool TileSet::load(SDL_Renderer *renderer, const std::string_view &filename, std::vector<tileinfo_t> tiledata)
{
    LOGI("extracting texture from %s", filename.data());
    CFileWrap file;
    if (!file.open(filename, "rb"))
    {
        LOGE("can't open %s", filename.data());
        return false;
    }

    CFrameSet set;
    if (!parsePNG(set, file, 0, true))
    {
        LOGE("fail to parse %s", filename.data());
        return false;
    }
    file.close();

    if (set.getSize() == 0)
        return false;

    m_frame = set.removeAt(0); // detach frame

    m_texture = createTexture(renderer, m_frame);
    if (!m_texture)
    {
        LOGE("create texture failed.");
        return false;
    }

    SDL_SetTextureScaleMode(m_texture, SDL_SCALEMODE_NEAREST);

    for (const auto &data : tiledata)
    {
        SDL_FRect rect{float(data.x), float(data.y),
                       static_cast<float>(data.w), static_cast<float>(data.h)};
        m_tiles.emplace_back(Tile{
            m_texture, rect, rect.w * m_scale, rect.h * m_scale});
    }

    return true;
}

bool TileSet::saveTiles(SDL_Texture *texture, std::vector<Tile> &tiles, CFrame *frame)
{
    int rows = frame->height() / m_sy;
    int cols = frame->width() / m_sx;
    tiles.clear();
    tiles.reserve(rows * cols);
    for (int y = 0; y < rows; ++y)
    {
        for (int x = 0; x < cols; ++x)
        {
            SDL_FRect rect{float(x * m_sx), float(y * m_sy),
                           static_cast<float>(m_sx), static_cast<float>(m_sy)};
            tiles.emplace_back(Tile{texture, rect, rect.w * m_scale, rect.h * m_scale});
        }
    }
    LOGI("TileSet::load() count %zu", m_tiles.size());
    return true;
}

SDL_Texture *TileSet::createTexture(SDL_Renderer *renderer, CFrame *frame)
{
    if (!renderer || !frame)
        return nullptr;

    // --- Create a single texture for the whole frame ---
    SDL_PropertiesID texProps = SDL_CreateProperties();
    SDL_SetNumberProperty(texProps, SDL_PROP_TEXTURE_CREATE_WIDTH_NUMBER, frame->width());
    SDL_SetNumberProperty(texProps, SDL_PROP_TEXTURE_CREATE_HEIGHT_NUMBER, frame->height());
    SDL_SetNumberProperty(texProps, SDL_PROP_TEXTURE_CREATE_FORMAT_NUMBER, SDL_PIXELFORMAT_ABGR8888);
    SDL_Texture *texture = SDL_CreateTextureWithProperties(renderer, texProps);
    SDL_DestroyProperties(texProps);

    if (!texture)
        return nullptr;

    const int pitch = frame->width() * sizeof(uint32_t); // 4 bytes per pixel
    if (!SDL_UpdateTexture(texture, nullptr, frame->getRGB().data(), pitch))
    {
        LOGE("SDL_UpdateTexture failed: %s", SDL_GetError());
        SDL_DestroyTexture(texture);
        return nullptr;
    }

    return texture;
}
